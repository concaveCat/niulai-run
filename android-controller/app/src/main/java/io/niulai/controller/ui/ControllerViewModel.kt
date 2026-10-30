package io.niulai.controller.ui

import android.app.Application
import android.content.Context
import android.media.AudioManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.ToneGenerator
import android.os.SystemClock
import android.util.Log
import io.niulai.controller.NiulaiApplication
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.niulai.controller.ble.AndroidBleController
import io.niulai.controller.ble.BleController
import io.niulai.controller.ble.BlePhase
import io.niulai.controller.ble.BleState
import io.niulai.controller.control.LegInputTracker
import io.niulai.controller.gameplay.GameplayEventKind
import io.niulai.controller.gameplay.AiFeedbackStatus
import io.niulai.controller.gameplay.GameplayFeedbackSummary
import io.niulai.controller.gameplay.GameplayFailureReason
import io.niulai.controller.gameplay.GameplayMode
import io.niulai.controller.gameplay.GameplayPhase
import io.niulai.controller.gameplay.GameplaySession
import io.niulai.controller.gameplay.GameplayUiState
import io.niulai.controller.protocol.BleProtocol
import io.niulai.controller.protocol.ControlPacket
import io.niulai.controller.protocol.Leg
import io.niulai.controller.feedback.GameplayFeedbackProvider
import io.niulai.controller.feedback.GameplayFeedbackSettings
import io.niulai.controller.feedback.GameplayFeedbackSettingsStore
import io.niulai.controller.feedback.FeedbackSettingsStore
import io.niulai.controller.feedback.HttpGameplayFeedbackProvider
import io.niulai.controller.feedback.localGameplayFeedback
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.sin

enum class HomeRequestStatus {
    IDLE,
    SENDING,
    SENT,
    FAILED,
}

data class ControllerUiState(
    val ble: BleState = BleState(),
    val txSequence: Int = 0,
    val txLegMask: Int = 0,
    val lastTxPacketHex: String = "01 00 00 00",
    val gameplay: GameplayUiState = GameplayUiState(),
    val feedbackSettings: GameplayFeedbackSettings = GameplayFeedbackSettings(),
    val homeRequestStatus: HomeRequestStatus = HomeRequestStatus.IDLE,
)

internal interface ControllerPreferences {
    fun getBoolean(key: String, defaultValue: Boolean): Boolean
    fun putBoolean(key: String, value: Boolean)
}

private class AndroidControllerPreferences(context: Context) : ControllerPreferences {
    // Keep the pre-seam namespace so existing AI-feedback preference values survive.
    private val preferences = context.getSharedPreferences("gameplay_feedback", Context.MODE_PRIVATE)

    override fun getBoolean(key: String, defaultValue: Boolean): Boolean =
        preferences.getBoolean(key, defaultValue)

    override fun putBoolean(key: String, value: Boolean) {
        preferences.edit().putBoolean(key, value).apply()
    }
}

class ControllerViewModel private constructor(
    application: Application,
    private val feedbackProviderOverride: GameplayFeedbackProvider?,
    private val bleController: BleController,
    private val controllerPreferences: ControllerPreferences,
    private val feedbackSettingsStore: FeedbackSettingsStore,
    private val nowMs: () -> Long,
) : AndroidViewModel(application) {
    constructor(application: Application) : this(
        application,
        null,
        AndroidBleController(application),
        AndroidControllerPreferences(application),
        GameplayFeedbackSettingsStore(application),
        { SystemClock.elapsedRealtime() },
    )

    /** Compatibility constructor used by tests and existing callers. */
    constructor(
        application: Application,
        feedbackProviderOverride: GameplayFeedbackProvider?,
    ) : this(
        application,
        feedbackProviderOverride,
        AndroidBleController(application),
        AndroidControllerPreferences(application),
        GameplayFeedbackSettingsStore(application),
        { SystemClock.elapsedRealtime() },
    )

    /** Host-test seam; production construction remains the Application overload. */
    internal constructor(
        application: Application,
        bleController: BleController,
        controllerPreferences: ControllerPreferences,
        feedbackSettingsStore: FeedbackSettingsStore,
        feedbackProviderOverride: GameplayFeedbackProvider? = null,
        nowMs: () -> Long = { SystemClock.elapsedRealtime() },
    ) : this(
        application,
        feedbackProviderOverride,
        bleController,
        controllerPreferences,
        feedbackSettingsStore,
        nowMs,
    )

    companion object {
        const val HEARTBEAT_INTERVAL_MS = 100L
        /* A platform GATT write can legitimately take longer than one
         * connection interval. HOME must wait for the neutral write callback,
         * otherwise the subsequent HOME write can race the old mask. */
        private const val CONTROL_WRITE_WAIT_TIMEOUT_MS = 2_000L
        /* HOME completion is reported after the firmware has accepted the
         * calibrated all-legs rest frame. If a notification is lost, this
         * bounded wait still gives the control task time to complete before
         * disconnect invokes the hard safety stop. */
        private const val HOME_COMPLETION_WAIT_TIMEOUT_MS = 1_500L
        private const val FEEDBACK_ENABLED_KEY = "ai_feedback_enabled"
    }

    private val input = LegInputTracker()
    private val controlState = kotlinx.coroutines.flow.MutableStateFlow(input.packet)
    private val feedbackSettingsState = kotlinx.coroutines.flow.MutableStateFlow(
        feedbackSettingsStore.read(),
    )
    private val homeRequestState = kotlinx.coroutines.flow.MutableStateFlow(HomeRequestStatus.IDLE)
    private val gameplayState = kotlinx.coroutines.flow.MutableStateFlow(
        GameplayUiState(
            aiFeedbackEnabled = controllerPreferences.getBoolean(FEEDBACK_ENABLED_KEY, true),
        ),
    )
    /* CONTROL_STATE is a latest-state stream. Keep at most one packet behind
     * the write currently in flight; replaying stale masks would make a
     * delayed BLE write look like slow or incorrect actuator motion. Any
     * completion-bearing packet that is displaced is explicitly failed so
     * HOME/disconnect callers cannot wait for a packet that was dropped. */
    private val outbound = Channel<OutboundPacket>(
        capacity = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST,
        onUndeliveredElement = { item ->
            item.completion?.complete(false)
        },
    )
    private val gameplayAudio = GameplayAudioFeedback()
    private var heartbeatJob: Job? = null
    private var gameplayTickerJob: Job? = null
    private var feedbackJob: Job? = null
    private var feedbackRequestId = 0L
    private var physicalEndJob: Job? = null
    private var gameplaySession: GameplaySession? = null
    private data class FinishedGameplay(
        val mode: GameplayMode,
        val isSimulated: Boolean,
    )
    private var finishedGameplay: FinishedGameplay? = null
    private var controllerActive = false

    val uiState: StateFlow<ControllerUiState> = combine(
        bleController.state,
        controlState,
        gameplayState,
        feedbackSettingsState,
        homeRequestState,
    ) { ble, control, gameplay, feedbackSettings, homeRequestStatus ->
        ControllerUiState(
            ble = ble,
            txSequence = control.sequence,
            txLegMask = control.legMask,
            lastTxPacketHex = BleProtocol.format(control.encode()),
            gameplay = gameplay,
            feedbackSettings = feedbackSettings,
            homeRequestStatus = homeRequestStatus,
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), ControllerUiState())

    init {
        viewModelScope.launch {
            for (item in outbound) {
                val sent = bleController.writeControl(item.packet.encode())
                item.completion?.complete(sent)
            }
        }
        viewModelScope.launch {
            var previous = bleController.state.value.phase
            bleController.state
                .map { it.phase }
                .distinctUntilChanged()
                .collect { phase ->
                    if (phase == BlePhase.CONNECTED) {
                        if (controllerActive) startHeartbeat()
                        queue(input.packet)
                    } else {
                        stopHeartbeat()
                        homeRequestState.value = HomeRequestStatus.IDLE
                        if (previous == BlePhase.CONNECTED || previous == BlePhase.CONNECTING) {
                            input.clear(forcePacket = false)?.let(::publishOnly)
                        }
                    }
                    previous = phase
                }
        }
        viewModelScope.launch {
            bleController.state
                .map { it.lastStatus?.failsafeActive == true }
                .distinctUntilChanged()
                .collect { failsafeActive ->
                    if (failsafeActive) {
                        // Do not let a stale local hold resume after the receiver
                        // reports its watchdog has forced the remote mask to zero.
                        input.clear(forcePacket = true)?.let(::publishAndQueue)
                    }
                }
        }
        viewModelScope.launch {
            bleController.state
                .map { it.lastStatus?.fallDetected == true }
                .distinctUntilChanged()
                .collect { fallDetected ->
                    if (fallDetected &&
                        gameplayState.value.phase == GameplayPhase.PLAYING &&
                        !gameplayState.value.isSimulated
                    ) {
                        finishGameplay(GameplayFailureReason.FALL)
                    }
                }
        }
        viewModelScope.launch {
            bleController.state
                .map { it.lastStatus }
                .distinctUntilChanged()
                .collect { status ->
                    if (status != null &&
                        gameplayState.value.phase == GameplayPhase.PLAYING &&
                        !gameplayState.value.isSimulated
                    ) {
                        gameplaySession?.recordStatus(
                            timestampMs = nowMs(),
                            appliedLegMask = status.appliedLegMask,
                            contactMask = status.contactMask,
                        )
                        gameplaySession?.let(::syncGameplaySnapshot)
                    }
                }
        }
    }

    fun startScan() = bleController.startScan()

    fun stopScan() = bleController.stopScan()

    fun connect(address: String) {
        input.clear(forcePacket = false)?.let(::publishOnly)
        bleController.connect(address)
    }

    fun disconnect() {
        physicalEndJob?.cancel()
        physicalEndJob = null
        stopHeartbeat()
        val neutral = input.clear(forcePacket = true) ?: input.packet
        publishOnly(neutral)
        viewModelScope.launch {
            if (bleController.state.value.phase == BlePhase.CONNECTED) {
                val completion = kotlinx.coroutines.CompletableDeferred<Boolean>()
                outbound.send(OutboundPacket(neutral, completion))
                withTimeoutOrNull(CONTROL_WRITE_WAIT_TIMEOUT_MS) { completion.await() }
            }
            bleController.disconnect()
        }
    }

    fun pointerDown(pointerId: Long, leg: Leg) {
        if (homeRequestState.value == HomeRequestStatus.SENDING) return
        input.pointerDown(pointerId, leg)?.let(::publishGameplayInput)
    }

    fun pointerUp(pointerId: Long) {
        if (homeRequestState.value == HomeRequestStatus.SENDING) return
        input.pointerUp(pointerId)?.let(::publishGameplayInput)
    }

    fun cancelLeg(leg: Leg) {
        if (homeRequestState.value == HomeRequestStatus.SENDING) return
        input.cancelLeg(leg)?.let(::publishGameplayInput)
    }

    /** Clears the local control preview without attempting a BLE write. */
    fun clearControls() {
        input.clear(forcePacket = true)?.let(::publishOnly)
    }

    /** Sends neutral first, then requests the firmware's authenticated home action. */
    fun requestHome() {
        if (bleController.state.value.phase != BlePhase.CONNECTED ||
            homeRequestState.value == HomeRequestStatus.SENDING) return
        homeRequestState.value = HomeRequestStatus.SENDING
        stopHeartbeat()
        val neutral = input.clear(forcePacket = true) ?: input.packet
        publishOnly(neutral)
        viewModelScope.launch {
            val completion = kotlinx.coroutines.CompletableDeferred<Boolean>()
            outbound.send(OutboundPacket(neutral, completion))
            val neutralSent = withTimeoutOrNull(CONTROL_WRITE_WAIT_TIMEOUT_MS) {
                completion.await()
            } == true
            val homeSent = neutralSent && bleController.writeHome()
            homeRequestState.value = if (homeSent) {
                HomeRequestStatus.SENT
            } else {
                HomeRequestStatus.FAILED
            }
            if (controllerActive && bleController.state.value.phase == BlePhase.CONNECTED) {
                startHeartbeat()
            }
        }
    }

    fun startTestGameplay() {
        startGameplaySession(
            isSimulated = true,
            mode = gameplayState.value.mode,
        )
    }

    /** Starts an observer-only session for the connected physical controller. */
    fun startPhysicalGameplay() {
        startGameplaySession(
            isSimulated = false,
            mode = gameplayState.value.mode,
        )
    }

    private fun startGameplaySession(isSimulated: Boolean, mode: GameplayMode) {
        if (gameplayState.value.phase == GameplayPhase.PLAYING) return
        val now = nowMs()
        feedbackJob?.cancel()
        feedbackRequestId += 1
        finishedGameplay = null
        gameplaySession = GameplaySession(mode = mode, startedAtMs = now)
        gameplayState.value = gameplayState.value.copy(
            phase = GameplayPhase.PLAYING,
            mode = mode,
            isSimulated = isSimulated,
            elapsedMs = 0L,
            progressPercent = 0,
            checkpoint = 0,
            completedSteps = 0,
            currentMask = 0,
            events = gameplaySession?.events.orEmpty(),
            result = null,
            aiFeedbackStatus = AiFeedbackStatus.IDLE,
            aiFeedback = null,
        )
        if (!isSimulated) {
            bleController.state.value.lastStatus?.let { status ->
                gameplaySession?.recordStatus(
                    timestampMs = nowMs(),
                    appliedLegMask = status.appliedLegMask,
                    contactMask = status.contactMask,
                )
            }
        }
        if (!isSimulated && bleController.state.value.lastStatus?.fallDetected == true) {
            finishGameplay(GameplayFailureReason.FALL)
            return
        }
        if (isSimulated) {
            gameplayAudio.onEvent(GameplayEventKind.SESSION_START)
            startGameplayTicker()
        }
    }

    fun endTestGameplay() {
        finishGameplay()
    }

    private fun finishGameplay(failureReason: GameplayFailureReason? = null) {
        val active = gameplaySession ?: return
        if (gameplayState.value.phase != GameplayPhase.PLAYING) return
        val isSimulated = gameplayState.value.isSimulated
        finishedGameplay = FinishedGameplay(
            mode = active.mode,
            isSimulated = isSimulated,
        )
        val result = active.finish(nowMs(), failureReason)
        val summary = active.feedbackSummary(result)
        stopGameplayTicker()
        gameplayState.value = gameplayState.value.copy(
            phase = GameplayPhase.RESULT,
            elapsedMs = result.elapsedMs,
            progressPercent = result.progressPercent,
            checkpoint = result.checkpoint,
            completedSteps = result.completedSteps,
            currentMask = 0,
            events = active.events,
            result = result,
            aiFeedbackStatus = if (gameplayState.value.aiFeedbackEnabled) {
                AiFeedbackStatus.LOADING
            } else {
                AiFeedbackStatus.IDLE
            },
            aiFeedback = null,
        )
        if (isSimulated) {
            gameplayAudio.onEvent(GameplayEventKind.SESSION_END)
        }
        val neutral = input.clear(forcePacket = true) ?: input.packet
        if (isSimulated) {
            publishOnly(neutral)
        } else {
            startPhysicalEndSequence(neutral)
        }
        if (gameplayState.value.aiFeedbackEnabled) {
            requestAiFeedback(summary)
        }
    }

    fun rematchGameplay() {
        val finished = finishedGameplay ?: gameplaySession?.let { session ->
            FinishedGameplay(
                mode = session.mode,
                isSimulated = gameplayState.value.isSimulated,
            )
        } ?: return
        startGameplaySession(
            isSimulated = finished.isSimulated,
            mode = finished.mode,
        )
    }

    fun toggleGameplayMode() {
        val mode = if (gameplayState.value.mode == GameplayMode.SOLO) {
                GameplayMode.SPLIT
            } else {
                GameplayMode.SOLO
            }
        gameplayState.value = gameplayState.value.copy(mode = mode)
        // The top-bar switch remains usable during an active round. Keep the
        // result/session metadata aligned with the mode shown to the player.
        gameplaySession?.mode = mode
        gameplayAudio.playModeToggleScale()
        if (bleController.state.value.phase == BlePhase.CONNECTED) {
            viewModelScope.launch {
                val modeValue = if (mode == GameplayMode.SPLIT) {
                    BleProtocol.GAME_MODE_MULTI
                } else {
                    BleProtocol.GAME_MODE_SOLO
                }
                bleController.writeGameMode(modeValue)
            }
        }
    }

    fun setGameplayMuted(muted: Boolean) {
        gameplayAudio.muted = muted
        gameplayState.value = gameplayState.value.copy(muted = muted)
    }

    fun setAiFeedbackEnabled(enabled: Boolean) {
        controllerPreferences.putBoolean(FEEDBACK_ENABLED_KEY, enabled)
        gameplayState.value = gameplayState.value.copy(
            aiFeedbackEnabled = enabled,
            aiFeedbackStatus = if (enabled) {
                AiFeedbackStatus.LOADING
            } else {
                AiFeedbackStatus.IDLE
            },
            aiFeedback = null,
        )
        feedbackJob?.cancel()
        feedbackRequestId += 1
        if (enabled && gameplayState.value.phase == GameplayPhase.RESULT) {
            val result = gameplayState.value.result
            val active = gameplaySession
            if (result != null && active != null) {
                requestAiFeedback(active.feedbackSummary(result))
            }
        }
    }

    fun saveFeedbackSettings(apiUrl: String, apiKey: String, model: String, useProxy: Boolean) {
        val settings = GameplayFeedbackSettings(
            apiUrl = apiUrl.trim(),
            apiKey = apiKey.trim(),
            model = model.trim(),
            useProxy = useProxy,
        )
        if (!feedbackSettingsStore.save(settings)) return
        feedbackSettingsState.value = settings
        feedbackJob?.cancel()
        feedbackRequestId += 1
        if (gameplayState.value.aiFeedbackEnabled && gameplayState.value.phase == GameplayPhase.RESULT) {
            val result = gameplayState.value.result
            val active = gameplaySession
            if (result != null && active != null) {
                requestAiFeedback(active.feedbackSummary(result))
            }
        }
    }

    fun leaveTestGameplay() {
        physicalEndJob?.cancel()
        physicalEndJob = null
        stopGameplayTicker()
        feedbackJob?.cancel()
        feedbackRequestId += 1
        gameplaySession = null
        finishedGameplay = null
        gameplayState.value = GameplayUiState(
            mode = gameplayState.value.mode,
            muted = gameplayState.value.muted,
            aiFeedbackEnabled = gameplayState.value.aiFeedbackEnabled,
        )
    }

    fun setControllerActive(active: Boolean) {
        if (controllerActive == active) return
        controllerActive = active
        bleController.requestHighPerformance(active)
        if (active && bleController.state.value.phase == BlePhase.CONNECTED) {
            startHeartbeat()
        } else if (!active) {
            stopHeartbeat()
            val neutral = input.clear(forcePacket = true) ?: input.packet
            publishAndQueue(neutral)
        }
    }

    override fun onCleared() {
        stopHeartbeat()
        stopGameplayTicker()
        gameplayAudio.close()
        outbound.close()
        bleController.close()
        super.onCleared()
    }

    private fun startHeartbeat() {
        if (heartbeatJob?.isActive == true) return
        heartbeatJob = viewModelScope.launch {
            while (bleController.state.value.phase == BlePhase.CONNECTED && controllerActive) {
                queue(input.packet)
                delay(HEARTBEAT_INTERVAL_MS)
            }
        }
    }

    private fun stopHeartbeat() {
        heartbeatJob?.cancel()
        heartbeatJob = null
    }

    private fun startGameplayTicker() {
        stopGameplayTicker()
        gameplayTickerJob = viewModelScope.launch {
            while (gameplayState.value.phase == GameplayPhase.PLAYING) {
                val active = gameplaySession ?: break
                val now = SystemClock.elapsedRealtime()
                gameplayState.value = gameplayState.value.copy(
                    elapsedMs = (now - active.events.first().timestampMs).coerceAtLeast(0L),
                    progressPercent = active.progressPercent,
                    checkpoint = active.completedSteps,
                    completedSteps = active.completedSteps,
                    currentMask = input.legMask,
                    events = active.events,
                )
                delay(100L)
            }
        }
    }

    private fun stopGameplayTicker() {
        gameplayTickerJob?.cancel()
        gameplayTickerJob = null
    }

    private fun requestAiFeedback(summary: GameplayFeedbackSummary) {
        feedbackJob?.cancel()
        val requestId = ++feedbackRequestId
        val settings = feedbackSettingsState.value
        val provider = feedbackProviderOverride ?: HttpGameplayFeedbackProvider(
            baseUrl = settings.apiUrl,
            apiKey = settings.apiKey,
            model = settings.model,
            useProxy = settings.useProxy,
        )
        val requestDebug = if (provider is HttpGameplayFeedbackProvider) {
            provider.debugDescription()
        } else {
            "测试评价提供器（未发起网络请求）"
        }
        gameplayState.value = gameplayState.value.copy(
            aiFeedbackStatus = AiFeedbackStatus.LOADING,
            aiFeedback = null,
        )
        Log.i(NiulaiApplication.LOG_TAG, "AI_FEEDBACK_REQUEST $requestDebug")
        feedbackJob = viewModelScope.launch {
            val result = provider.generate(summary)
            if (requestId != feedbackRequestId || !gameplayState.value.aiFeedbackEnabled) return@launch
            val generated = result.getOrNull()
            if (generated.isNullOrBlank()) {
                val reason = result.exceptionOrNull()?.message
                    ?.replace(Regex("\\s+"), " ")
                    ?.trim()
                    ?.take(240)
                    ?: "服务返回空内容"
                val debug = "$requestDebug；失败=$reason；已回退本地评价"
                Log.e(NiulaiApplication.LOG_TAG, "AI_FEEDBACK_FAILURE $debug")
                gameplayState.value = gameplayState.value.copy(
                    aiFeedbackStatus = AiFeedbackStatus.FALLBACK,
                    aiFeedback = localGameplayFeedback(summary),
                )
            } else {
                val debug = "$requestDebug；成功"
                Log.i(NiulaiApplication.LOG_TAG, "AI_FEEDBACK_SUCCESS $debug")
                gameplayState.value = gameplayState.value.copy(
                    aiFeedbackStatus = AiFeedbackStatus.SUCCESS,
                    aiFeedback = generated,
                )
            }
        }
    }

    private fun publishAndQueue(packet: ControlPacket) {
        publishOnly(packet)
        recordGameplayMask(packet.legMask)
        queue(packet)
    }

    private fun publishGameplayInput(packet: ControlPacket) {
        publishOnly(packet)
        recordGameplayMask(packet.legMask)
        /* The simulated control page can coexist with a previously connected
         * test controller. Never let its button presses reach the real BLE
         * endpoint while a simulated round is active. */
        if (gameplayState.value.phase != GameplayPhase.PLAYING ||
            !gameplayState.value.isSimulated
        ) {
            queue(packet)
        }
    }

    private fun startPhysicalEndSequence(neutral: ControlPacket) {
        physicalEndJob?.cancel()
        physicalEndJob = viewModelScope.launch {
            stopHeartbeat()
            val connected = bleController.state.value.phase == BlePhase.CONNECTED
            if (!connected) return@launch

            homeRequestState.value = HomeRequestStatus.SENDING
            val completion = kotlinx.coroutines.CompletableDeferred<Boolean>()
            outbound.send(OutboundPacket(neutral, completion))
            val neutralSent = withTimeoutOrNull(CONTROL_WRITE_WAIT_TIMEOUT_MS) {
                completion.await()
            } == true
            val previousNotification = bleController.state.value.lastNotificationHex
            val homeSent = neutralSent && bleController.writeHome()
            if (homeSent) {
                awaitHomeCompletion(previousNotification)
            }
            homeRequestState.value = if (homeSent) {
                HomeRequestStatus.SENT
            } else {
                HomeRequestStatus.FAILED
            }
            /* This is intentionally unconditional after the bounded HOME
             * phase. Firmware disconnect handling closes /OE and puts the
             * PCA9685 through its safe-stop sequence even if STATUS was lost. */
            bleController.disconnect()
        }
    }

    private suspend fun awaitHomeCompletion(previousNotification: String?): Boolean =
        withTimeoutOrNull(HOME_COMPLETION_WAIT_TIMEOUT_MS) {
            bleController.state
                .map { state ->
                    state.lastNotificationHex to state.lastStatus?.homeComplete
                }
                .filter { (notification, complete) ->
                    complete == true && notification != null &&
                        notification != previousNotification
                }
                .first()
            true
        } ?: false

    private fun recordGameplayMask(mask: Int) {
        if (gameplayState.value.phase != GameplayPhase.PLAYING) return
        val active = gameplaySession ?: return
        val previousMask = gameplayState.value.currentMask
        // Start audio before session/state bookkeeping so the press callback
        // reaches the preloaded AudioTrack at the earliest possible moment.
        if (gameplayState.value.isSimulated) {
            gameplayAudio.onLegMaskChanged(mask and previousMask.inv())
        }
        active.recordMask(
            timestampMs = nowMs(),
            legMask = mask,
            simulateStep = gameplayState.value.isSimulated,
        )
        syncGameplaySnapshot(active, currentMask = mask)
    }

    private fun syncGameplaySnapshot(active: GameplaySession, currentMask: Int = input.legMask) {
        gameplayState.value = gameplayState.value.copy(
            currentMask = currentMask,
            progressPercent = active.progressPercent,
            checkpoint = active.completedSteps,
            completedSteps = active.completedSteps,
            events = active.events,
        )
    }

    private fun publishOnly(packet: ControlPacket) {
        controlState.value = packet
    }

    private fun queue(packet: ControlPacket) {
        if (bleController.state.value.phase == BlePhase.CONNECTED) {
            outbound.trySend(OutboundPacket(packet))
        }
    }

    private data class OutboundPacket(
        val packet: ControlPacket,
        val completion: kotlinx.coroutines.CompletableDeferred<Boolean>? = null,
    )

    fun playMama() {
        if (bleController.state.value.phase != BlePhase.CONNECTED) return
        viewModelScope.launch {
            bleController.playMama()
        }
    }
}

private class GameplayAudioFeedback : AutoCloseable {
    @Volatile
    var muted: Boolean = false
    private val tone = runCatching { ToneGenerator(AudioManager.STREAM_MUSIC, 70) }.getOrNull()
    private val pianoNotes = PIANO_LEG_HZ.map(::synthesizePianoNote).toTypedArray()
    /** Preloaded tracks avoid creating an AudioTrack on the button's critical path. */
    private val pianoTracks = pianoNotes.map(::createPianoTrack).toTypedArray()
    private var lastToneAt = 0L
    @Volatile
    private var closed = false

    fun onEvent(kind: GameplayEventKind) {
        if (muted) return
        when (kind) {
            GameplayEventKind.SESSION_START -> play(ToneGenerator.TONE_PROP_ACK, 70)
            GameplayEventKind.SESSION_END -> play(ToneGenerator.TONE_PROP_BEEP, 100)
            else -> Unit
        }
    }

    /** Starts a note only for newly pressed legs; releasing a leg is silent. */
    fun onLegMaskChanged(pressedMask: Int) {
        if (muted || closed) return
        PIANO_LEG_HZ.indices.forEach { legIndex ->
            if (pressedMask and (1 shl legIndex) != 0) {
                playPianoNote(legIndex)
            }
        }
    }

    /** Plays scale degrees 1, 3, 5 and 8 as a short piano-like arpeggio. */
    fun playModeToggleScale() {
        if (muted || closed) return
        PIANO_LEG_HZ.indices.forEach(::playPianoNote)
    }

    /** Retriggers an already-loaded static track directly from the press callback. */
    private fun playPianoNote(noteIndex: Int) {
        val track = pianoTracks.getOrNull(noteIndex) ?: return
        synchronized(track) {
            if (muted || closed) return
            runCatching {
                if (track.playState == AudioTrack.PLAYSTATE_PLAYING) track.stop()
                track.setPlaybackHeadPosition(0)
                track.play()
            }
        }
    }

    private fun createPianoTrack(samples: ShortArray): AudioTrack? {
        val track = runCatching {
            AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .setLegacyStreamType(AudioManager.STREAM_MUSIC)
                        .build(),
                )
                .setAudioFormat(
                    AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setSampleRate(PIANO_SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                        .build(),
                )
                .setBufferSizeInBytes(samples.size * Short.SIZE_BYTES)
                .setTransferMode(AudioTrack.MODE_STATIC)
                .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
                .build()
        }.getOrNull() ?: return null

        return runCatching {
            track.write(samples, 0, samples.size)
            track.setVolume(0.85f)
            track
        }.getOrElse {
            track.release()
            null
        }
    }

    private fun play(toneType: Int, durationMs: Int) {
        val now = SystemClock.elapsedRealtime()
        if (now - lastToneAt < 120L) return
        lastToneAt = now
        tone?.startTone(toneType, durationMs)
    }

    override fun close() {
        closed = true
        pianoTracks.forEach { track ->
            track?.let {
                runCatching { it.stop() }
                it.release()
            }
        }
        tone?.release()
    }

    private fun synthesizePianoNote(frequency: Double): ShortArray {
        val noteSamples = PIANO_SAMPLE_RATE * PIANO_NOTE_MS / 1_000
        val raw = DoubleArray(noteSamples)
        var peak = 0.0
        repeat(noteSamples) { sampleIndex ->
            val time = sampleIndex.toDouble() / PIANO_SAMPLE_RATE
            val attack = 1.0 - exp(-PIANO_ATTACK_RATE * time)
            val body = 0.78 * exp(-PIANO_BODY_DECAY * time) +
                0.22 * exp(-PIANO_BRIGHT_DECAY * time)
            val release = if (sampleIndex > noteSamples - PIANO_RELEASE_SAMPLES) {
                (noteSamples - sampleIndex).toDouble() / PIANO_RELEASE_SAMPLES
            } else {
                1.0
            }
            val envelope = attack * body * release
            var sample = 0.0

            PIANO_VOICE_RATIOS.forEachIndexed { voiceIndex, voiceRatio ->
                var voice = 0.0
                PIANO_PARTIAL_AMPLITUDES.forEachIndexed { partialIndex, amplitude ->
                    val partial = partialIndex + 1
                    val inharmonicRatio = kotlin.math.sqrt(
                        1.0 + PIANO_INHARMONICITY * partial * partial,
                    )
                    val partialFrequency = frequency * partial * inharmonicRatio * voiceRatio
                    val partialDecay = exp(-time * (PIANO_PARTIAL_DECAY + partial * 0.22))
                    voice += sin(2.0 * PI * partialFrequency * time) * amplitude * partialDecay
                }
                sample += voice * PIANO_VOICE_WEIGHTS[voiceIndex]
            }

            // A short, bright hammer transient makes the attack read as a struck key.
            val hammerEnvelope = exp(-PIANO_HAMMER_DECAY * time)
            // Cosine phase gives the hammer transient a non-zero first sample;
            // a sine starting at phase zero otherwise sounds late even when
            // AudioTrack.play() is called immediately.
            val hammer = (
                kotlin.math.cos(2.0 * PI * frequency * 7.2 * time) * 0.055 +
                    kotlin.math.cos(2.0 * PI * frequency * 11.7 * time) * 0.026
                ) * hammerEnvelope
            raw[sampleIndex] = (sample * envelope) + hammer
            peak = maxOf(peak, kotlin.math.abs(raw[sampleIndex]))
        }

        val output = ShortArray(noteSamples)
        repeat(noteSamples) { sampleIndex ->
            val normalized = if (peak > 0.0) raw[sampleIndex] / peak else 0.0
            output[sampleIndex] = (normalized * PIANO_OUTPUT_GAIN * Short.MAX_VALUE)
                .coerceIn(Short.MIN_VALUE.toDouble(), Short.MAX_VALUE.toDouble())
                .toInt()
                .toShort()
        }
        return output
    }

    private companion object {
        const val PIANO_SAMPLE_RATE = 44_100
        const val PIANO_NOTE_MS = 620
        const val PIANO_ATTACK_RATE = 420.0
        const val PIANO_BODY_DECAY = 1.85
        const val PIANO_BRIGHT_DECAY = 7.5
        const val PIANO_PARTIAL_DECAY = 0.9
        const val PIANO_HAMMER_DECAY = 125.0
        const val PIANO_OUTPUT_GAIN = 0.72
        val PIANO_RELEASE_SAMPLES = PIANO_SAMPLE_RATE * 70 / 1_000
        val PIANO_VOICE_RATIOS = doubleArrayOf(0.9987, 1.0, 1.0013)
        val PIANO_VOICE_WEIGHTS = doubleArrayOf(0.24, 0.52, 0.24)
        val PIANO_PARTIAL_AMPLITUDES = doubleArrayOf(
            1.00, 0.54, 0.31, 0.19, 0.125, 0.085,
            0.058, 0.040, 0.028, 0.020, 0.014, 0.010,
        )
        const val PIANO_INHARMONICITY = 0.00042
        val PIANO_LEG_HZ = doubleArrayOf(261.63, 329.63, 392.00, 523.25)
    }
}
