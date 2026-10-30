package io.niulai.controller.ui

import android.app.Application
import io.niulai.controller.ble.BleController
import io.niulai.controller.ble.BlePhase
import io.niulai.controller.ble.BleState
import io.niulai.controller.feedback.FeedbackSettingsStore
import io.niulai.controller.feedback.GameplayFeedbackSettings
import io.niulai.controller.protocol.BleProtocol
import io.niulai.controller.protocol.Leg
import io.niulai.controller.protocol.StatusPacket
import io.niulai.controller.gameplay.GameplayMode
import io.niulai.controller.gameplay.GameplayPhase
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestDispatcher
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

private class FakeBleController : BleController {
    private val _state = MutableStateFlow(BleState())
    override val state: StateFlow<BleState> = _state.asStateFlow()
    val writes = mutableListOf<ByteArray>()
    val gameModeWrites = mutableListOf<Int>()
    val homeWrites = mutableListOf<ByteArray>()
    var beforeControlWrite: (() -> Unit)? = null
    var controlWriteDelayMs = 0L
    var blockControlWrites = false
    var releaseControlWrites = kotlinx.coroutines.CompletableDeferred<Unit>()
    var completeHomeOnWrite = false
    var highPerformanceRequests = mutableListOf<Boolean>()
    var disconnectCount = 0

    fun emit(next: BleState) {
        _state.value = next
    }

    override fun startScan() {
        emit(_state.value.copy(phase = BlePhase.SCANNING, error = null))
    }

    override fun stopScan() {
        if (_state.value.phase == BlePhase.SCANNING) {
            emit(_state.value.copy(phase = BlePhase.DISCONNECTED))
        }
    }

    override fun connect(address: String) {
        emit(
            _state.value.copy(
                phase = BlePhase.CONNECTING,
                deviceName = address,
                error = null,
            ),
        )
    }

    override fun disconnect() {
        disconnectCount++
        emit(
            _state.value.copy(
                phase = BlePhase.DISCONNECTED,
                deviceName = null,
                lastStatus = null,
                lastNotificationHex = null,
            ),
        )
    }

    override fun requestHighPerformance(enabled: Boolean) {
        highPerformanceRequests += enabled
    }

    override suspend fun writeControl(packet: ByteArray): Boolean {
        beforeControlWrite?.invoke()
        if (blockControlWrites) releaseControlWrites.await()
        if (controlWriteDelayMs > 0L) delay(controlWriteDelayMs)
        writes += packet.copyOf()
        return true
    }

    override suspend fun writeHome(): Boolean {
        homeWrites += BleProtocol.encodeHomeCommand()
        if (completeHomeOnWrite) {
            emit(
                _state.value.copy(
                    lastStatus = StatusPacket(
                        sequence = _state.value.lastStatus?.sequence ?: 0,
                        appliedLegMask = 0,
                        flags = BleProtocol.STATUS_HOME_COMPLETE,
                    ),
                    lastNotificationHex = "01 00 00 40",
                ),
            )
        }
        return true
    }

    override suspend fun writeGameMode(mode: Int): Boolean {
        gameModeWrites += mode
        return true
    }

    override suspend fun playMama(): Boolean = true

    override fun close() = Unit
}

private class MemoryControllerPreferences : ControllerPreferences {
    private val values = mutableMapOf<String, Boolean>()

    override fun getBoolean(key: String, defaultValue: Boolean): Boolean =
        values[key] ?: defaultValue

    override fun putBoolean(key: String, value: Boolean) {
        values[key] = value
    }
}

private class MemoryFeedbackSettingsStore : FeedbackSettingsStore {
    private var settings = GameplayFeedbackSettings(
        apiUrl = "http://mock.invalid/v1",
        apiKey = "",
        model = "mock-model",
        useProxy = true,
    )

    override fun read(): GameplayFeedbackSettings = settings

    override fun save(settings: GameplayFeedbackSettings): Boolean {
        this.settings = settings
        return true
    }
}

@OptIn(ExperimentalCoroutinesApi::class)
class ControllerViewModelBleTest {
    private lateinit var dispatcher: TestDispatcher

    @Before
    fun setUp() {
        dispatcher = StandardTestDispatcher()
        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun connectedInputAndFailsafeProduceSafePacketsAndUiState() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }

        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        assertEquals(1, fake.writes.size)
        assertPacket(fake.writes[0], sequence = 0, mask = 0)

        viewModel.pointerDown(pointerId = 1, leg = Leg.FRONT_LEFT)
        advanceUntilIdle()
        assertPacket(fake.writes.last(), sequence = 1, mask = 0x01)
        assertEquals(0x01, viewModel.uiState.value.txLegMask)

        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(
                    sequence = 1,
                    appliedLegMask = 0,
                    flags = BleProtocol.STATUS_FAILSAFE_ACTIVE,
                ),
            ),
        )
        advanceUntilIdle()
        assertPacket(fake.writes.last(), sequence = 2, mask = 0)
        assertEquals(0, viewModel.uiState.value.txLegMask)
        assertEquals(2, viewModel.uiState.value.txSequence)
        assertTrue(viewModel.uiState.value.ble.lastStatus?.failsafeActive == true)

        collector.cancel()
    }

    @Test
    fun fallStatusFinishesPhysicalGameplayWithZeroScores() = runTest(dispatcher) {
        val fake = FakeBleController()
        val preferences = MemoryControllerPreferences().also {
            it.putBoolean("ai_feedback_enabled", false)
        }
        val viewModel = ControllerViewModel(
            Application(),
            fake,
            preferences,
            MemoryFeedbackSettingsStore(),
            nowMs = { 1_000L },
        )
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }

        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        viewModel.startPhysicalGameplay()
        advanceUntilIdle()

        fake.emit(
            fake.state.value.copy(
                lastStatus = StatusPacket(
                    sequence = 1,
                    appliedLegMask = 0,
                    flags = BleProtocol.STATUS_FALL_DETECTED,
                ),
            ),
        )
        advanceUntilIdle()

        val result = viewModel.uiState.value.gameplay.result
        assertEquals(GameplayPhase.RESULT, viewModel.uiState.value.gameplay.phase)
        assertTrue(result?.failed == true)
        assertEquals(0, result?.speedScore)
        assertEquals(0, result?.coordinationScore)
        collector.cancel()
    }

    @Test
    fun disconnectSendsNeutralPacketBeforeDroppingConnection() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        viewModel.pointerDown(pointerId = 9, leg = Leg.REAR_RIGHT)
        advanceUntilIdle()

        viewModel.disconnect()
        advanceUntilIdle()

        assertPacket(fake.writes.last(), sequence = 2, mask = 0)
        assertEquals(1, fake.disconnectCount)
        assertEquals(BlePhase.DISCONNECTED, fake.state.value.phase)
        assertEquals(0, viewModel.uiState.value.txLegMask)
        assertFalse(viewModel.uiState.value.ble.lastStatus != null)
        collector.cancel()
    }

    @Test
    fun endingPhysicalGameplayHomesBeforeDisconnectingAndKeepsResultVisible() =
        runTest(dispatcher) {
            val fake = FakeBleController().also { it.completeHomeOnWrite = true }
            val preferences = MemoryControllerPreferences().also {
                it.putBoolean("ai_feedback_enabled", false)
            }
            val viewModel = ControllerViewModel(
                Application(),
                fake,
                preferences,
                MemoryFeedbackSettingsStore(),
                nowMs = { 1_000L },
            )
            val collector = backgroundScope.launch { viewModel.uiState.collect {} }
            fake.emit(
                BleState(
                    phase = BlePhase.CONNECTED,
                    deviceName = "Mock Niulai",
                    lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
                    lastNotificationHex = "01 00 00 00",
                ),
            )
            advanceUntilIdle()

            viewModel.startPhysicalGameplay()
            viewModel.pointerDown(pointerId = 11, leg = Leg.FRONT_LEFT)
            advanceUntilIdle()
            viewModel.endTestGameplay()
            advanceUntilIdle()

            assertPacket(fake.writes.last(), sequence = 2, mask = 0)
            assertArrayEquals(byteArrayOf(0x01, 0x01), fake.homeWrites.single())
            assertEquals(1, fake.disconnectCount)
            assertEquals(GameplayPhase.RESULT, viewModel.uiState.value.gameplay.phase)
            assertFalse(viewModel.uiState.value.gameplay.isSimulated)
            collector.cancel()
        }

    @Test
    fun lifecycleDeactivationClearsActiveHoldWithoutHeartbeatLeak() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        viewModel.pointerDown(pointerId = 4, leg = Leg.FRONT_RIGHT)
        advanceUntilIdle()

        viewModel.setControllerActive(true)
        viewModel.setControllerActive(false)
        advanceUntilIdle()

        assertPacket(fake.writes.last(), sequence = 2, mask = 0)
        assertEquals(listOf(true, false), fake.highPerformanceRequests)
        assertEquals(0, viewModel.uiState.value.txLegMask)
        collector.cancel()
    }

    @Test
    fun blockedWriterDropsStalePacketsAndSendsLatestMask() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()

        fake.blockControlWrites = true
        viewModel.pointerDown(pointerId = 1, leg = Leg.FRONT_LEFT)
        runCurrent()
        viewModel.pointerDown(pointerId = 2, leg = Leg.FRONT_RIGHT)
        viewModel.pointerUp(pointerId = 2)
        fake.blockControlWrites = false
        fake.releaseControlWrites.complete(Unit)
        advanceUntilIdle()

        assertEquals(3, fake.writes.size)
        assertPacket(fake.writes[1], sequence = 1, mask = 0x01)
        assertPacket(fake.writes[2], sequence = 3, mask = 0x01)
        collector.cancel()
    }

    @Test
    fun homeSendsNeutralBeforeAuthenticatedHomeCommand() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        viewModel.pointerDown(pointerId = 7, leg = Leg.FRONT_LEFT)
        advanceUntilIdle()

        viewModel.requestHome()
        advanceUntilIdle()

        assertPacket(fake.writes.last(), sequence = 2, mask = 0)
        assertArrayEquals(byteArrayOf(0x01, 0x01), fake.homeWrites.single())
        assertEquals(HomeRequestStatus.SENT, viewModel.uiState.value.homeRequestStatus)
        assertEquals(0, viewModel.uiState.value.txLegMask)
        collector.cancel()
    }

    @Test
    fun homeWaitsForNeutralWriteLongerThanOneConnectionInterval() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        viewModel.pointerDown(pointerId = 7, leg = Leg.FRONT_LEFT)
        advanceUntilIdle()

        /* 500 ms is longer than the old 200 ms HOME neutralization wait but
         * still within the controller's normal 2 s GATT write timeout. */
        fake.controlWriteDelayMs = 500L
        viewModel.requestHome()
        advanceUntilIdle()

        assertPacket(fake.writes.last(), sequence = 2, mask = 0)
        assertArrayEquals(byteArrayOf(0x01, 0x01), fake.homeWrites.single())
        assertEquals(HomeRequestStatus.SENT, viewModel.uiState.value.homeRequestStatus)
        collector.cancel()
    }

    @Test
    fun homeRejectsInputArrivingBetweenNeutralAndHomeWrites() = runTest(dispatcher) {
        val fake = FakeBleController()
        val viewModel = testViewModel(fake)
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()
        fake.beforeControlWrite = { viewModel.pointerDown(pointerId = 88, leg = Leg.REAR_LEFT) }

        viewModel.requestHome()
        advanceUntilIdle()

        assertEquals(1, fake.homeWrites.size)
        assertPacket(fake.writes.last(), sequence = 1, mask = 0)
        assertEquals(0, viewModel.uiState.value.txLegMask)
        collector.cancel()
    }

    @Test
    fun rematchPreservesTheFinishedModeAndPhysicalSessionType() = runTest(dispatcher) {
        val fake = FakeBleController()
        val preferences = MemoryControllerPreferences().also {
            it.putBoolean("ai_feedback_enabled", false)
        }
        val viewModel = ControllerViewModel(
            Application(),
            fake,
            preferences,
            MemoryFeedbackSettingsStore(),
            nowMs = { 1_000L },
        )
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()

        viewModel.toggleGameplayMode()
        advanceUntilIdle()
        viewModel.startPhysicalGameplay()
        advanceUntilIdle()
        assertEquals(GameplayPhase.PLAYING, viewModel.uiState.value.gameplay.phase)
        assertEquals(GameplayMode.SPLIT, viewModel.uiState.value.gameplay.mode)
        assertFalse(viewModel.uiState.value.gameplay.isSimulated)

        viewModel.endTestGameplay()
        advanceUntilIdle()
        // The result page must restart the finished round, even if the
        // current mode value changes before the rematch callback runs.
        viewModel.toggleGameplayMode()
        advanceUntilIdle()
        viewModel.rematchGameplay()
        advanceUntilIdle()

        assertEquals(GameplayPhase.PLAYING, viewModel.uiState.value.gameplay.phase)
        assertEquals(GameplayMode.SPLIT, viewModel.uiState.value.gameplay.mode)
        assertFalse(viewModel.uiState.value.gameplay.isSimulated)
        collector.cancel()
    }

    @Test
    fun topBarModeToggleRemainsUsableDuringPhysicalGameplay() = runTest(dispatcher) {
        val fake = FakeBleController()
        val preferences = MemoryControllerPreferences().also {
            it.putBoolean("ai_feedback_enabled", false)
        }
        val viewModel = ControllerViewModel(
            Application(),
            fake,
            preferences,
            MemoryFeedbackSettingsStore(),
            nowMs = { 1_000L },
        )
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }
        fake.emit(
            BleState(
                phase = BlePhase.CONNECTED,
                deviceName = "Mock Niulai",
                lastStatus = StatusPacket(sequence = 0, appliedLegMask = 0, flags = 0),
            ),
        )
        advanceUntilIdle()

        viewModel.startPhysicalGameplay()
        advanceUntilIdle()
        assertEquals(GameplayPhase.PLAYING, viewModel.uiState.value.gameplay.phase)
        assertEquals(GameplayMode.SOLO, viewModel.uiState.value.gameplay.mode)

        viewModel.toggleGameplayMode()
        advanceUntilIdle()
        assertEquals(GameplayMode.SPLIT, viewModel.uiState.value.gameplay.mode)
        assertEquals(listOf(BleProtocol.GAME_MODE_MULTI), fake.gameModeWrites)

        viewModel.toggleGameplayMode()
        advanceUntilIdle()
        assertEquals(GameplayMode.SOLO, viewModel.uiState.value.gameplay.mode)
        assertEquals(
            listOf(BleProtocol.GAME_MODE_MULTI, BleProtocol.GAME_MODE_SOLO),
            fake.gameModeWrites,
        )

        viewModel.endTestGameplay()
        advanceUntilIdle()
        assertEquals(GameplayMode.SOLO, viewModel.uiState.value.gameplay.mode)
        collector.cancel()
    }

    @Test
    fun rematchPreservesFinishedSoloMode() = runTest(dispatcher) {
        val fake = FakeBleController()
        val preferences = MemoryControllerPreferences().also {
            it.putBoolean("ai_feedback_enabled", false)
        }
        val viewModel = ControllerViewModel(
            Application(),
            fake,
            preferences,
            MemoryFeedbackSettingsStore(),
            nowMs = { 1_000L },
        )
        val collector = backgroundScope.launch { viewModel.uiState.collect {} }

        viewModel.startPhysicalGameplay()
        advanceUntilIdle()
        assertEquals(GameplayMode.SOLO, viewModel.uiState.value.gameplay.mode)

        viewModel.endTestGameplay()
        advanceUntilIdle()
        viewModel.toggleGameplayMode()
        advanceUntilIdle()
        assertEquals(GameplayMode.SPLIT, viewModel.uiState.value.gameplay.mode)

        viewModel.rematchGameplay()
        advanceUntilIdle()

        assertEquals(GameplayPhase.PLAYING, viewModel.uiState.value.gameplay.phase)
        assertEquals(GameplayMode.SOLO, viewModel.uiState.value.gameplay.mode)
        assertFalse(viewModel.uiState.value.gameplay.isSimulated)
        collector.cancel()
    }

    private fun testViewModel(fake: FakeBleController): ControllerViewModel =
        ControllerViewModel(
            Application(),
            fake,
            MemoryControllerPreferences(),
            MemoryFeedbackSettingsStore(),
        )

    private fun assertPacket(packet: ByteArray, sequence: Int, mask: Int) {
        assertArrayEquals(
            byteArrayOf(0x01, sequence.toByte(), mask.toByte(), 0x00),
            packet,
        )
    }
}
