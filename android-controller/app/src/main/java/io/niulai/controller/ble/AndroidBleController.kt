package io.niulai.controller.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.BroadcastReceiver
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import androidx.annotation.RequiresPermission
import io.niulai.controller.NiulaiApplication
import io.niulai.controller.protocol.BleProtocol
import io.niulai.controller.protocol.NiulaiGatt
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

@SuppressLint("MissingPermission")
class AndroidBleController(private val context: Context) : BleController {
    private val manager = context.getSystemService(BluetoothManager::class.java)
    private val adapter: BluetoothAdapter? = manager?.adapter
    private val writeMutex = Mutex()
    private val discovered = linkedMapOf<String, NiulaiDevice>()
    private val mainHandler = Handler(Looper.getMainLooper())

    private val _state = MutableStateFlow(
        BleState(
            phase = if (isBleSupported()) BlePhase.DISCONNECTED else BlePhase.UNSUPPORTED,
            error = if (isBleSupported()) null else "Bluetooth LE is not supported on this device",
        ),
    )
    override val state: StateFlow<BleState> = _state.asStateFlow()

    private var gatt: BluetoothGatt? = null
    private var controlCharacteristic: BluetoothGattCharacteristic? = null
    private var statusCharacteristic: BluetoothGattCharacteristic? = null
    private var homeCharacteristic: BluetoothGattCharacteristic? = null
    private var telemetryCharacteristic: BluetoothGattCharacteristic? = null
    private var gameModeCharacteristic: BluetoothGattCharacteristic? = null
    private var audioCommandCharacteristic: BluetoothGattCharacteristic? = null
    private var pendingWriteCharacteristic: java.util.UUID? = null
    private var pendingWrite: CompletableDeferred<Boolean>? = null
    private var encryptedProbePending = false
    private var bondReceiverRegistered = false
    @Volatile private var highPerformanceRequested = false
    private var scanTimeoutTask: Runnable? = null
    private var connectionTimeoutTask: Runnable? = null

    private companion object {
        const val SCAN_TIMEOUT_MS = 10_000L
        const val CONNECTION_TIMEOUT_MS = 12_000L
        const val WRITE_TIMEOUT_MS = 2_000L
    }

    private val bondReceiver = object : BroadcastReceiver() {
        override fun onReceive(receiverContext: Context, intent: Intent) {
            if (intent.action != BluetoothDevice.ACTION_BOND_STATE_CHANGED) return
            val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                intent.getParcelableExtra(
                    BluetoothDevice.EXTRA_DEVICE,
                    BluetoothDevice::class.java,
                )
            } else {
                @Suppress("DEPRECATION")
                intent.getParcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)
            } ?: return
            val currentGatt = gatt ?: return
            if (device.address != currentGatt.device.address) return

            when (intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.BOND_NONE)) {
                BluetoothDevice.BOND_BONDED -> beginEncryptedProbe(currentGatt)
                BluetoothDevice.BOND_NONE -> {
                    if (_state.value.phase == BlePhase.CONNECTING) {
                        failConnection("BLE pairing failed", currentGatt)
                    }
                }
            }
        }
    }

    init {
        try {
            val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                context.registerReceiver(bondReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
            } else {
                @Suppress("DEPRECATION")
                context.registerReceiver(bondReceiver, filter)
            }
            bondReceiverRegistered = true
        } catch (_: SecurityException) {
            Log.w(NiulaiApplication.LOG_TAG, "BLE_BOND_RECEIVER_PERMISSION_DENIED")
        }
    }

    private fun isBleSupported(): Boolean =
        adapter != null && context.packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            recordDevice(result)
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            results.forEach(::recordDevice)
        }

        override fun onScanFailed(errorCode: Int) {
            if (_state.value.phase != BlePhase.SCANNING) return
            cancelScanTimeout()
            Log.e(NiulaiApplication.LOG_TAG, "BLE_SCAN_FAILED code=$errorCode")
            _state.update {
                it.copy(phase = BlePhase.DISCONNECTED, error = "BLE scan failed: $errorCode")
            }
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failConnection("GATT connection failed: $status", gatt)
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    this@AndroidBleController.gatt = gatt
                    _state.update {
                        it.copy(
                            phase = BlePhase.CONNECTING,
                            lastStatus = null,
                            lastTelemetry = null,
                            lastNotificationHex = null,
                            lastTelemetryHex = null,
                            error = null,
                        )
                    }
                    try {
                        gatt.requestConnectionPriority(
                            if (highPerformanceRequested) {
                                BluetoothGatt.CONNECTION_PRIORITY_HIGH
                            } else {
                                BluetoothGatt.CONNECTION_PRIORITY_BALANCED
                            },
                        )
                    } catch (_: SecurityException) {
                        failConnection("BLE connection priority permission denied", gatt)
                        return
                    }
                    val cacheRefreshRequested = refreshGattCache(gatt)
                    Log.i(
                        NiulaiApplication.LOG_TAG,
                        "BLE_GATT_CACHE_REFRESH requested=$cacheRefreshRequested",
                    )
                    val discoveryStarted = try {
                        gatt.discoverServices()
                    } catch (_: SecurityException) {
                        false
                    }
                    if (!discoveryStarted) {
                        failConnection("Unable to start GATT service discovery", gatt)
                    }
                }

                BluetoothProfile.STATE_DISCONNECTED -> {
                    closeGatt(gatt)
                    _state.update {
                        it.copy(
                            phase = BlePhase.DISCONNECTED,
                            deviceName = null,
                            lastStatus = null,
                            lastNotificationHex = null,
                        )
                    }
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failConnection("GATT service discovery failed: $status", gatt)
                return
            }

            logDiscoveredGatt(gatt)
            val service = gatt.getService(NiulaiGatt.SERVICE_UUID)
            val control = service?.getCharacteristic(NiulaiGatt.CONTROL_STATE_UUID)
            val statusValue = service?.getCharacteristic(NiulaiGatt.STATUS_UUID)
            val home = service?.getCharacteristic(NiulaiGatt.HOME_COMMAND_UUID)
            val telemetry = service?.getCharacteristic(NiulaiGatt.IMU_TELEMETRY_UUID)
            val audio = service?.getCharacteristic(NiulaiGatt.AUDIO_COMMAND_UUID)
            if (control == null || statusValue == null || home == null || telemetry == null ||
                audio == null) {
                failConnection(
                    "牛来快跑 GATT service or runtime characteristics are missing; see BLE_GATT_DISCOVERED in Logcat",
                    gatt,
                )
                return
            }

            val controlRequired = BluetoothGattCharacteristic.PROPERTY_WRITE
            val statusRequired = BluetoothGattCharacteristic.PROPERTY_READ or
                BluetoothGattCharacteristic.PROPERTY_NOTIFY
            val telemetryRequired = statusRequired
            if (control.properties and controlRequired != controlRequired) {
                failConnection("CONTROL_STATE does not support WRITE", gatt)
                return
            }
            if (statusValue.properties and statusRequired != statusRequired) {
                failConnection("STATUS does not support READ and NOTIFY", gatt)
                return
            }
            if (home.properties and controlRequired != controlRequired) {
                failConnection("HOME_COMMAND does not support WRITE", gatt)
                return
            }
            if (telemetry.properties and telemetryRequired != telemetryRequired) {
                failConnection("IMU_TELEMETRY does not support READ and NOTIFY", gatt)
                return
            }
            if (audio.properties and controlRequired != controlRequired) {
                failConnection("AUDIO_COMMAND does not support WRITE", gatt)
                return
            }

            controlCharacteristic = control
            statusCharacteristic = statusValue
            homeCharacteristic = home
            telemetryCharacteristic = telemetry
            gameModeCharacteristic = service?.getCharacteristic(NiulaiGatt.GAME_MODE_UUID)
            audioCommandCharacteristic = audio
            if (!enableStatusNotifications(gatt, statusValue)) {
                failConnection("Unable to subscribe to STATUS notifications", gatt)
            }
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int,
        ) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            if (descriptor.uuid != NiulaiGatt.CLIENT_CONFIGURATION_UUID) return
            val statusDescriptor = statusCharacteristic?.getDescriptor(
                NiulaiGatt.CLIENT_CONFIGURATION_UUID,
            )
            val telemetryDescriptor = telemetryCharacteristic?.getDescriptor(
                NiulaiGatt.CLIENT_CONFIGURATION_UUID,
            )
            if (descriptor !== statusDescriptor && descriptor !== telemetryDescriptor) return
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failConnection(
                    if (descriptor === telemetryDescriptor) {
                        "IMU_TELEMETRY notification subscription failed: $status"
                    } else {
                        "STATUS notification subscription failed: $status"
                    },
                    gatt,
                )
                return
            }
            if (descriptor === statusDescriptor) {
                val characteristic = telemetryCharacteristic
                if (characteristic == null || !enableTelemetryNotifications(gatt, characteristic)) {
                    failConnection("Unable to subscribe to IMU_TELEMETRY notifications", gatt)
                }
            } else {
                val characteristic = statusCharacteristic
                if (characteristic == null || !readInitialStatus(gatt, characteristic)) {
                    failConnection("Unable to read initial STATUS packet", gatt)
                }
            }
        }

        @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            if (characteristic.uuid != NiulaiGatt.STATUS_UUID) return
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failConnection("Initial STATUS read failed: $status", gatt)
                return
            }
            val value = characteristic.value
            if (value == null) {
                failConnection("Initial STATUS read returned no value", gatt)
                return
            }
            if (!applyStatusPacket(gatt, value, "read")) return
            beginEncryptedProbe(gatt)
        }

        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
            status: Int,
        ) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            if (characteristic.uuid != NiulaiGatt.STATUS_UUID) return
            if (status != BluetoothGatt.GATT_SUCCESS) {
                failConnection("Initial STATUS read failed: $status", gatt)
                return
            }
            if (!applyStatusPacket(gatt, value, "read")) return
            beginEncryptedProbe(gatt)
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            if (characteristic.uuid != NiulaiGatt.CONTROL_STATE_UUID &&
                characteristic.uuid != NiulaiGatt.HOME_COMMAND_UUID &&
                characteristic.uuid != NiulaiGatt.GAME_MODE_UUID &&
                characteristic.uuid != NiulaiGatt.AUDIO_COMMAND_UUID) return
            if (encryptedProbePending) {
                encryptedProbePending = false
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    markReady(gatt)
                } else {
                    failConnection("Authenticated CONTROL_STATE probe failed: $status", gatt)
                }
            } else {
                val completion = pendingWrite
                if (completion != null && pendingWriteCharacteristic == characteristic.uuid) {
                    pendingWrite = null
                    pendingWriteCharacteristic = null
                    completion.complete(status == BluetoothGatt.GATT_SUCCESS)
                }
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    val operation = when (characteristic.uuid) {
                        NiulaiGatt.HOME_COMMAND_UUID -> "HOME_COMMAND"
                        NiulaiGatt.GAME_MODE_UUID -> "GAME_MODE"
                        NiulaiGatt.AUDIO_COMMAND_UUID -> "AUDIO_COMMAND"
                        else -> "CONTROL_STATE"
                    }
                    val statusName = gattStatusName(status)
                    val message = "$operation write failed: $status ($statusName)"
                    Log.e(
                        NiulaiApplication.LOG_TAG,
                        "BLE_GATT_WRITE_FAILED operation=$operation status=$status name=$statusName",
                    )
                    /* An application-level ATT rejection is not necessarily a
                     * broken link. Keep the encrypted connection alive so a
                     * caller can recover/retry instead of turning status 14
                     * into an apparent app exit. */
                    _state.update { it.copy(error = message) }
                }
            }
        }

        @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
        ) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            handleNotification(gatt, characteristic.uuid, characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            if (!isCurrentGatt(gatt)) {
                closeGatt(gatt)
                return
            }
            handleNotification(gatt, characteristic.uuid, value)
        }
    }

    @RequiresPermission(
        anyOf = [Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_ADMIN],
    )
    override fun startScan() {
        if (!isBleSupported()) {
            _state.update { it.copy(phase = BlePhase.UNSUPPORTED) }
            return
        }
        if (adapter?.isEnabled != true) {
            _state.update { it.copy(phase = BlePhase.DISCONNECTED, error = "Bluetooth is disabled") }
            return
        }
        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            _state.update { it.copy(error = "BLE scanner is unavailable") }
            return
        }

        stopScan()
        discovered.clear()
        _state.update {
            it.copy(phase = BlePhase.SCANNING, devices = emptyList(), error = null)
        }
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        // Some Android scan implementations do not apply service filters to a
        // service UUID delivered in the Scan Response. Receive results first,
        // then retain only Niulai candidates in recordDevice().
        try {
            scanner.startScan(emptyList(), settings, scanCallback)
        } catch (_: SecurityException) {
            _state.update {
                it.copy(phase = BlePhase.DISCONNECTED, error = "BLE scan permission denied")
            }
            return
        }
        scheduleScanTimeout()
        Log.i(NiulaiApplication.LOG_TAG, "BLE_SCAN_STARTED filtering=app_side")
    }

    override fun stopScan() {
        cancelScanTimeout()
        try {
            adapter?.bluetoothLeScanner?.stopScan(scanCallback)
        } catch (_: SecurityException) {
            Log.w(NiulaiApplication.LOG_TAG, "BLE_SCAN_STOP_PERMISSION_DENIED")
        }
        if (_state.value.phase == BlePhase.SCANNING) {
            _state.update { it.copy(phase = BlePhase.DISCONNECTED) }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    override fun connect(address: String) {
        stopScan()
        disconnectGatt()
        val device = try {
            adapter?.getRemoteDevice(address)
        } catch (_: IllegalArgumentException) {
            null
        } catch (_: SecurityException) {
            _state.update {
                it.copy(phase = BlePhase.DISCONNECTED, error = "BLE connect permission denied")
            }
            return
        }
        if (device == null) {
            _state.update { it.copy(phase = BlePhase.DISCONNECTED, error = "Invalid BLE device") }
            return
        }

        val name = discovered[address]?.name ?: safeDeviceName(device)
        _state.update { it.copy(phase = BlePhase.CONNECTING, deviceName = name, error = null) }
        val createdGatt = try {
            device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (_: SecurityException) {
            null
        }
        gatt = createdGatt
        if (createdGatt == null) {
            _state.update { it.copy(phase = BlePhase.DISCONNECTED, error = "Unable to create GATT") }
        } else {
            scheduleConnectionTimeout(createdGatt)
        }
    }

    override fun disconnect() {
        stopScan()
        cancelConnectionTimeout()
        val current = gatt
        if (current == null) {
            _state.update { it.copy(phase = BlePhase.DISCONNECTED, deviceName = null) }
        } else {
            try {
                current.disconnect()
            } catch (_: SecurityException) {
                Log.w(NiulaiApplication.LOG_TAG, "BLE_DISCONNECT_PERMISSION_DENIED")
            }
            closeGatt(current)
            _state.update {
                it.copy(
                    phase = BlePhase.DISCONNECTED,
                    deviceName = null,
                            lastStatus = null,
                            lastTelemetry = null,
                            lastNotificationHex = null,
                            lastTelemetryHex = null,
                )
            }
        }
    }

    override fun requestHighPerformance(enabled: Boolean) {
        highPerformanceRequested = enabled
        try {
            gatt?.requestConnectionPriority(
                if (enabled) {
                    BluetoothGatt.CONNECTION_PRIORITY_HIGH
                } else {
                    BluetoothGatt.CONNECTION_PRIORITY_BALANCED
                },
            )
        } catch (_: SecurityException) {
            _state.update { it.copy(error = "BLE connection priority permission denied") }
        }
    }

    override suspend fun writeControl(packet: ByteArray): Boolean =
        writeEncryptedCharacteristic(NiulaiGatt.CONTROL_STATE_UUID, packet)

    override suspend fun writeHome(): Boolean =
        writeEncryptedCharacteristic(
            NiulaiGatt.HOME_COMMAND_UUID,
            BleProtocol.encodeHomeCommand(),
        )

    override suspend fun writeGameMode(mode: Int): Boolean =
        writeEncryptedCharacteristic(
            NiulaiGatt.GAME_MODE_UUID,
            BleProtocol.encodeGameMode(mode),
        )

    override suspend fun playMama(): Boolean =
        writeEncryptedCharacteristic(
            NiulaiGatt.AUDIO_COMMAND_UUID,
            BleProtocol.encodePlayMamaCommand(),
        )

    private fun beginEncryptedProbe(targetGatt: BluetoothGatt) {
        if (!isCurrentGatt(targetGatt) || _state.value.phase != BlePhase.CONNECTING ||
            encryptedProbePending) return
        val device = targetGatt.device
        when (device.bondState) {
            BluetoothDevice.BOND_BONDED -> {
                val characteristic = controlCharacteristic
                if (characteristic == null) {
                    failConnection("CONTROL_STATE is unavailable for encrypted-link probe", targetGatt)
                    return
                }
                encryptedProbePending = true
                val probe = BleProtocol.encodeControl(sequence = 0, legMask = 0)
                if (!writeGattCharacteristic(targetGatt, characteristic, probe)) {
                    encryptedProbePending = false
                    failConnection("Unable to start encrypted CONTROL_STATE probe", targetGatt)
                }
            }
            BluetoothDevice.BOND_BONDING -> {
                _state.update { it.copy(error = "BLE pairing in progress; approve the system prompt") }
            }
            else -> {
                _state.update { it.copy(error = "BLE pairing required; approve the system prompt (no PIN)") }
                val started = try {
                    device.createBond()
                } catch (_: SecurityException) {
                    false
                }
                if (!started && device.bondState != BluetoothDevice.BOND_BONDING) {
                    failConnection("Unable to start BLE pairing", targetGatt)
                }
            }
        }
    }

    private suspend fun writeEncryptedCharacteristic(
        characteristicUuid: java.util.UUID,
        payload: ByteArray,
    ): Boolean = writeMutex.withLock {
        if (_state.value.phase != BlePhase.CONNECTED) return@withLock false
        val currentGatt = gatt ?: return@withLock false
        val characteristic = when (characteristicUuid) {
            NiulaiGatt.CONTROL_STATE_UUID -> controlCharacteristic
            NiulaiGatt.HOME_COMMAND_UUID -> homeCharacteristic
            NiulaiGatt.GAME_MODE_UUID -> gameModeCharacteristic
            NiulaiGatt.AUDIO_COMMAND_UUID -> audioCommandCharacteristic
            else -> null
        } ?: return@withLock false
        val completion = CompletableDeferred<Boolean>()
        pendingWriteCharacteristic = characteristicUuid
        pendingWrite = completion
        if (!writeGattCharacteristic(currentGatt, characteristic, payload.copyOf())) {
            pendingWrite = null
            pendingWriteCharacteristic = null
            return@withLock false
        }
        val result = withTimeoutOrNull(WRITE_TIMEOUT_MS) { completion.await() } ?: false
        if (pendingWrite === completion) {
            pendingWrite = null
            pendingWriteCharacteristic = null
        }
        result
    }

    private fun writeGattCharacteristic(
        currentGatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        payload: ByteArray,
    ): Boolean = try {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            currentGatt.writeCharacteristic(
                characteristic,
                payload,
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            ) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            @Suppress("DEPRECATION")
            characteristic.value = payload
            @Suppress("DEPRECATION")
            currentGatt.writeCharacteristic(characteristic)
        }
    } catch (_: SecurityException) {
        _state.update { it.copy(error = "BLE write permission denied") }
        false
    }

    override fun close() {
        stopScan()
        disconnectGatt()
        if (bondReceiverRegistered) {
            try {
                context.unregisterReceiver(bondReceiver)
            } catch (_: IllegalArgumentException) {
                // Already unregistered by the owning context.
            }
            bondReceiverRegistered = false
        }
    }

    private fun recordDevice(result: ScanResult) {
        val device = result.device
        val name = result.scanRecord?.deviceName ?: safeDeviceName(device)
        val advertisedServices = result.scanRecord?.serviceUuids.orEmpty()
        val hasNiulaiService = ParcelUuid(NiulaiGatt.SERVICE_UUID) in advertisedServices
        val hasNiulaiName = name == "牛来快跑"
        if (!hasNiulaiService && !hasNiulaiName) return

        discovered[device.address] = NiulaiDevice(device.address, name)
        Log.i(
            NiulaiApplication.LOG_TAG,
            "BLE_SCAN_CANDIDATE address=${device.address} service=$hasNiulaiService name=$hasNiulaiName",
        )
        _state.update { it.copy(devices = discovered.values.sortedBy(NiulaiDevice::name)) }
    }

    private fun safeDeviceName(device: BluetoothDevice): String = device.name ?: "牛来快跑 (${device.address})"

    /**
     * Android has no public API to invalidate a BluetoothGatt service cache.
     * The hidden refresh() method is available on the platform BluetoothGatt
     * implementation and is needed when a firmware update adds characteristics
     * without changing the service UUID. Discovery still proceeds if a vendor
     * build does not expose the method.
     */
    private fun refreshGattCache(gatt: BluetoothGatt): Boolean = try {
        val refresh = gatt.javaClass.getMethod("refresh")
        (refresh.invoke(gatt) as? Boolean) == true
    } catch (error: ReflectiveOperationException) {
        Log.w(
            NiulaiApplication.LOG_TAG,
            "BLE_GATT_CACHE_REFRESH unavailable=${error.javaClass.simpleName}",
        )
        false
    } catch (error: SecurityException) {
        Log.w(
            NiulaiApplication.LOG_TAG,
            "BLE_GATT_CACHE_REFRESH denied=${error.javaClass.simpleName}",
        )
        false
    }

    private fun logDiscoveredGatt(gatt: BluetoothGatt) {
        val services = gatt.services.joinToString(separator = "; ") { service ->
            val characteristics = service.characteristics.joinToString(separator = ",") { characteristic ->
                "${characteristic.uuid}(props=0x${characteristic.properties.toString(16)})"
            }
            "${service.uuid}[$characteristics]"
        }
        Log.i(NiulaiApplication.LOG_TAG, "BLE_GATT_DISCOVERED services=$services")
    }

    private fun enableStatusNotifications(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
    ): Boolean = enableNotifications(gatt, characteristic)

    private fun enableTelemetryNotifications(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
    ): Boolean = enableNotifications(gatt, characteristic)

    private fun enableNotifications(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
    ): Boolean = try {
        if (!gatt.setCharacteristicNotification(characteristic, true)) {
            false
        } else {
            val descriptor = characteristic.getDescriptor(NiulaiGatt.CLIENT_CONFIGURATION_UUID)
            if (descriptor == null) {
                false
            } else {
                val enableValue = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    gatt.writeDescriptor(descriptor, enableValue) == BluetoothStatusCodes.SUCCESS
                } else {
                    @Suppress("DEPRECATION")
                    descriptor.value = enableValue
                    @Suppress("DEPRECATION")
                    gatt.writeDescriptor(descriptor)
                }
            }
        }
    } catch (_: SecurityException) {
        false
    }

    private fun readInitialStatus(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
    ): Boolean = try {
        // The one-argument API remains the only Boolean-compatible overload in
        // the SDK stubs used by this project; it is valid on all supported API
        // levels and still delivers the modern callback overload on API 33+.
        @Suppress("DEPRECATION")
        gatt.readCharacteristic(characteristic)
    } catch (_: SecurityException) {
        false
    }

    private fun handleNotification(
        gatt: BluetoothGatt,
        uuid: java.util.UUID,
        value: ByteArray,
    ) {
        when (uuid) {
            NiulaiGatt.STATUS_UUID -> applyStatusPacket(gatt, value, "notification")
            NiulaiGatt.IMU_TELEMETRY_UUID -> applyTelemetryPacket(gatt, value)
        }
    }

    private fun applyTelemetryPacket(gatt: BluetoothGatt, value: ByteArray): Boolean {
        if (!isCurrentGatt(gatt)) return false
        return try {
            val telemetry = BleProtocol.decodeTelemetry(value)
            _state.update {
                it.copy(
                    lastTelemetry = telemetry,
                    lastTelemetryHex = BleProtocol.format(value),
                    error = null,
                )
            }
            true
        } catch (error: IllegalArgumentException) {
            failConnection("Invalid IMU_TELEMETRY packet: ${error.message}", gatt)
            false
        }
    }

    private fun applyStatusPacket(
        gatt: BluetoothGatt,
        value: ByteArray,
        source: String,
    ): Boolean {
        if (!isCurrentGatt(gatt)) return false
        try {
            val packet = BleProtocol.decodeStatus(value)
            _state.update {
                it.copy(
                    lastStatus = packet,
                    lastNotificationHex = BleProtocol.format(value),
                    error = null,
                )
            }
            return true
        } catch (error: IllegalArgumentException) {
            failConnection("Invalid STATUS packet from $source: ${error.message}", gatt)
            return false
        }
    }

    private fun markReady(targetGatt: BluetoothGatt) {
        if (!isCurrentGatt(targetGatt)) return
        cancelConnectionTimeout()
        _state.update { it.copy(phase = BlePhase.CONNECTED, error = null) }
        Log.i(NiulaiApplication.LOG_TAG, "BLE_GATT_READY initial_status=1 notifications=1")
    }

    private fun failConnection(message: String, targetGatt: BluetoothGatt) {
        if (!isCurrentGatt(targetGatt)) {
            closeGatt(targetGatt)
            return
        }
        cancelConnectionTimeout()
        closeGatt(targetGatt)
        _state.update {
            it.copy(
                phase = BlePhase.DISCONNECTED,
                deviceName = null,
                lastStatus = null,
                lastTelemetry = null,
                lastNotificationHex = null,
                lastTelemetryHex = null,
                error = message,
            )
        }
    }

    private fun gattStatusName(status: Int): String = when (status) {
        BluetoothGatt.GATT_SUCCESS -> "GATT_SUCCESS"
        14 -> "ATT_UNLIKELY"
        133 -> "GATT_ERROR"
        else -> "UNKNOWN"
    }

    private fun disconnectGatt() {
        cancelConnectionTimeout()
        val current = gatt ?: return
        try {
            current.disconnect()
        } catch (_: SecurityException) {
            Log.w(NiulaiApplication.LOG_TAG, "BLE_DISCONNECT_PERMISSION_DENIED")
        }
        closeGatt(current)
    }

    private fun closeGatt(targetGatt: BluetoothGatt) {
        targetGatt.close()
        if (gatt === targetGatt) {
            encryptedProbePending = false
            gatt = null
            controlCharacteristic = null
            statusCharacteristic = null
            homeCharacteristic = null
            telemetryCharacteristic = null
            gameModeCharacteristic = null
            audioCommandCharacteristic = null
            pendingWrite?.complete(false)
            pendingWrite = null
            pendingWriteCharacteristic = null
        }
    }

    private fun isCurrentGatt(candidate: BluetoothGatt): Boolean = gatt === candidate

    private fun scheduleScanTimeout() {
        cancelScanTimeout()
        val task = Runnable {
            if (_state.value.phase != BlePhase.SCANNING) return@Runnable
            try {
                adapter?.bluetoothLeScanner?.stopScan(scanCallback)
            } catch (_: SecurityException) {
                Log.w(NiulaiApplication.LOG_TAG, "BLE_SCAN_TIMEOUT_STOP_PERMISSION_DENIED")
            }
            scanTimeoutTask = null
            _state.update {
                it.copy(phase = BlePhase.DISCONNECTED, error = "BLE scan timed out")
            }
            Log.w(NiulaiApplication.LOG_TAG, "BLE_SCAN_TIMEOUT")
        }
        scanTimeoutTask = task
        mainHandler.postDelayed(task, SCAN_TIMEOUT_MS)
    }

    private fun cancelScanTimeout() {
        scanTimeoutTask?.let(mainHandler::removeCallbacks)
        scanTimeoutTask = null
    }

    private fun scheduleConnectionTimeout(targetGatt: BluetoothGatt) {
        cancelConnectionTimeout()
        val task = Runnable {
            if (!isCurrentGatt(targetGatt) || _state.value.phase != BlePhase.CONNECTING) return@Runnable
            failConnection("GATT connection timed out", targetGatt)
            Log.w(NiulaiApplication.LOG_TAG, "BLE_GATT_TIMEOUT")
        }
        connectionTimeoutTask = task
        mainHandler.postDelayed(task, CONNECTION_TIMEOUT_MS)
    }

    private fun cancelConnectionTimeout() {
        connectionTimeoutTask?.let(mainHandler::removeCallbacks)
        connectionTimeoutTask = null
    }
}
