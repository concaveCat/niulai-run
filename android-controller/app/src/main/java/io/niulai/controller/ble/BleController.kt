package io.niulai.controller.ble

import io.niulai.controller.protocol.StatusPacket
import io.niulai.controller.protocol.AccelerationTelemetry
import kotlinx.coroutines.flow.StateFlow

enum class BlePhase {
    UNSUPPORTED,
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    CONNECTED,
}

data class NiulaiDevice(
    val address: String,
    val name: String,
)

data class BleState(
    val phase: BlePhase = BlePhase.DISCONNECTED,
    val devices: List<NiulaiDevice> = emptyList(),
    val deviceName: String? = null,
    val lastStatus: StatusPacket? = null,
    val lastTelemetry: AccelerationTelemetry? = null,
    val lastNotificationHex: String? = null,
    val lastTelemetryHex: String? = null,
    val error: String? = null,
)

interface BleController : AutoCloseable {
    val state: StateFlow<BleState>

    fun startScan()
    fun stopScan()
    fun connect(address: String)
    fun disconnect()
    fun requestHighPerformance(enabled: Boolean)
    suspend fun writeControl(packet: ByteArray): Boolean
    suspend fun writeHome(): Boolean
    suspend fun writeGameMode(mode: Int): Boolean
    suspend fun playMama(): Boolean
}
