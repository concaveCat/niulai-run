package io.niulai.controller.gameplay

import kotlin.math.abs
import kotlin.math.roundToInt

enum class GameplayMode {
    SOLO,
    SPLIT,
}

enum class GameplayPhase {
    IDLE,
    PLAYING,
    RESULT,
}

enum class GameplayFailureReason {
    FALL,
}

enum class GameplayEventKind {
    SESSION_START,
    INPUT_MASK,
    SESSION_END,
}

/** All telemetry in this v0.1 test loop is deliberately marked as simulated. */
data class MockTelemetry(
    val state: String,
    val contact: Boolean,
    val progressPercent: Int,
    val checkpoint: Int,
)

data class GameplayEvent(
    val timestampMs: Long,
    val kind: GameplayEventKind,
    val legMask: Int,
    val changedMask: Int,
    val telemetry: MockTelemetry,
)

data class GameplayResult(
    val elapsedMs: Long,
    val progressPercent: Int,
    val checkpoint: Int,
    val completedSteps: Int = 0,
    val eventCount: Int,
    val speedScore: Int?,
    val coordinationScore: Int,
    val highlight: String,
    val suggestion: String,
    val diagnosticTimestampMs: Long?,
    val failureReason: GameplayFailureReason? = null,
) {
    val failed: Boolean
        get() = failureReason != null

    val scoreReady: Boolean
        get() = completedSteps >= GameplayScoringConfig.MIN_FORMAL_STEPS
}

/** Privacy-safe aggregate sent to the optional AI feedback provider. */
data class GameplayFeedbackSummary(
    val mode: GameplayMode,
    val elapsedMs: Long,
    val progressPercent: Int,
    val checkpoint: Int,
    val completedSteps: Int = 0,
    val speedScore: Int?,
    val coordinationScore: Int,
    val eventCount: Int,
    val legPressCounts: List<Int>,
    val rhythmConsistencyPercent: Int,
    val highlight: String,
    val suggestion: String,
    val failureReason: GameplayFailureReason? = null,
)

enum class AiFeedbackStatus {
    IDLE,
    LOADING,
    SUCCESS,
    FALLBACK,
}

data class GameplayUiState(
    val phase: GameplayPhase = GameplayPhase.IDLE,
    val mode: GameplayMode = GameplayMode.SOLO,
    val isSimulated: Boolean = true,
    val elapsedMs: Long = 0L,
    val progressPercent: Int = 0,
    val checkpoint: Int = 0,
    val completedSteps: Int = 0,
    val currentMask: Int = 0,
    val events: List<GameplayEvent> = emptyList(),
    val result: GameplayResult? = null,
    val muted: Boolean = false,
    val aiFeedbackEnabled: Boolean = true,
    val aiFeedbackStatus: AiFeedbackStatus = AiFeedbackStatus.IDLE,
    val aiFeedback: String? = null,
)

object GameplayScoringConfig {
    const val STEP_TARGET = 8
    const val MIN_FORMAL_STEPS = 3
    const val FAST_STEP_MS = 900L
    const val SLOW_STEP_MS = 3_000L
    const val CONTINUITY_FULL_MS = 500L
    const val CONTINUITY_ZERO_MS = 3_000L
    const val RHYTHM_FULL_MAD_RATIO = 0.50
    const val ATTEMPT_TIMEOUT_MS = 3_500L
    const val CONTACT_DEBOUNCE_MS = 200L
}

/**
 * Deterministic observer-side session model. It never emits control commands.
 * Timestamps are supplied by the caller so tests can reproduce it.
 */
class GameplaySession(
    var mode: GameplayMode,
    private val startedAtMs: Long,
) {
    private class LegAttempt {
        var requested = false
        var accepted = false
        var airborne = false
        var released = false
        var startedAtMs = 0L
        var contactWasHighAtStart = false

        fun reset() {
            requested = false
            accepted = false
            airborne = false
            released = false
            startedAtMs = 0L
            contactWasHighAtStart = false
        }
    }

    private val recordedEvents = mutableListOf<GameplayEvent>()
    private var previousMask = 0
    private var inputEventCount = 0
    private val legAttempts = Array(4) { LegAttempt() }
    private val lastCompletedAtMs = LongArray(4) { Long.MIN_VALUE }
    private var previousContactMask: Int? = null
    private var attemptedCount = 0
    private var acceptedCount = 0
    private val completedStepTimes = mutableListOf<Long>()
    private val completedStepLegs = mutableListOf<Int>()

    init {
        recordedEvents += GameplayEvent(
            timestampMs = startedAtMs,
            kind = GameplayEventKind.SESSION_START,
            legMask = 0,
            changedMask = 0,
            telemetry = telemetryFor(inputEventCount, 0),
        )
    }

    val events: List<GameplayEvent>
        get() = recordedEvents.toList()

    /**
     * Records a control-mask change. Test mode passes simulateStep=true so a
     * press/release pair is a deterministic mock step. Entity mode passes
     * false and waits for recordStatus() to confirm a real contact cycle.
     */
    fun recordMask(timestampMs: Long, legMask: Int, simulateStep: Boolean = true) {
        require(timestampMs >= startedAtMs) { "event timestamp cannot precede session start" }
        require(legMask and 0x0f.inv() == 0) { "leg mask contains reserved bits" }
        if (legMask == previousMask) return

        inputEventCount += 1
        val changedMask = previousMask xor legMask
        val risingMask = legMask and previousMask.inv() and 0x0f
        val fallingMask = previousMask and legMask.inv() and 0x0f
        for (index in 0 until 4) {
            val bit = 1 shl index
            val attempt = legAttempts[index]
            if (risingMask and bit != 0) {
                attempt.reset()
                attempt.requested = true
                attempt.startedAtMs = timestampMs
                attempt.contactWasHighAtStart = previousContactMask?.let { it and bit != 0 } == true
                attemptedCount++
                if (simulateStep) {
                    attempt.accepted = true
                    acceptedCount++
                }
            }
            if (fallingMask and bit != 0 && attempt.requested) {
                attempt.released = true
                if (simulateStep) completeStep(index, timestampMs)
            }
        }
        previousMask = legMask
        recordedEvents += GameplayEvent(
            timestampMs = timestampMs,
            kind = GameplayEventKind.INPUT_MASK,
            legMask = legMask,
            changedMask = changedMask,
            telemetry = telemetryFor(inputEventCount, legMask),
        )
    }

    /**
     * Consumes the read-only firmware STATUS snapshot for an entity session.
     * A valid step requires an accepted command, observed departure from
     * contact, command release, and a subsequent contact return.
     */
    fun recordStatus(timestampMs: Long, appliedLegMask: Int, contactMask: Int) {
        require(timestampMs >= startedAtMs) { "status timestamp cannot precede session start" }
        require(appliedLegMask and 0x0f.inv() == 0) {
            "applied mask contains reserved flags"
        }
        require(contactMask and 0x0f.inv() == 0) {
            "contact mask contains reserved flags"
        }

        expirePendingAttempts(timestampMs)
        val oldContact = previousContactMask ?: contactMask
        for (index in 0 until 4) {
            val bit = 1 shl index
            val attempt = legAttempts[index]
            val applied = appliedLegMask and bit != 0
            val contact = contactMask and bit != 0
            val contactRose = contact && (oldContact and bit) == 0
            val contactFell = !contact && (oldContact and bit) != 0

            if (attempt.requested && applied && !attempt.accepted) {
                attempt.accepted = true
                acceptedCount++
            }
            if (attempt.requested && attempt.accepted && !applied) {
                attempt.released = true
            }
            if (attempt.accepted && attempt.contactWasHighAtStart && !contact) {
                attempt.airborne = true
            }
            if (attempt.accepted && contactFell) {
                attempt.airborne = true
            }
            if (attempt.requested && attempt.airborne && attempt.released && contactRose) {
                completeStep(index, timestampMs)
            }
        }
        previousContactMask = contactMask
    }

    val completedSteps: Int
        get() = completedStepTimes.size

    val progressPercent: Int
        get() = (completedSteps * 100 / GameplayScoringConfig.STEP_TARGET)
            .coerceAtMost(100)

    fun finish(
        timestampMs: Long,
        failureReason: GameplayFailureReason? = null,
    ): GameplayResult {
        require(timestampMs >= startedAtMs) { "finish timestamp cannot precede session start" }
        expirePendingAttempts(timestampMs)
        val finalProgress = progressPercent
        val finalTelemetry = telemetryFor(inputEventCount, 0)
        recordedEvents += GameplayEvent(
            timestampMs = timestampMs,
            kind = GameplayEventKind.SESSION_END,
            legMask = previousMask,
            changedMask = 0,
            telemetry = finalTelemetry.copy(progressPercent = finalProgress),
        )
        return score(
            timestampMs,
            finalProgress,
            completedSteps,
            failureReason,
        )
    }

    fun feedbackSummary(result: GameplayResult): GameplayFeedbackSummary =
        GameplayFeedbackSummary(
            mode = mode,
            elapsedMs = result.elapsedMs,
            progressPercent = result.progressPercent,
            checkpoint = result.checkpoint,
            completedSteps = result.completedSteps,
            speedScore = result.speedScore,
            coordinationScore = result.coordinationScore,
            eventCount = result.eventCount,
            legPressCounts = listOf(
                pressCount(0x01),
                pressCount(0x02),
                pressCount(0x04),
                pressCount(0x08),
            ),
            rhythmConsistencyPercent = rhythmScore().roundToInt().coerceIn(0, 100),
            highlight = result.highlight,
            suggestion = result.suggestion,
            failureReason = result.failureReason,
        )

    private fun pressCount(bit: Int): Int = recordedEvents
        .asSequence()
        .filter { it.kind == GameplayEventKind.INPUT_MASK }
        .count { it.changedMask and it.legMask and bit != 0 }

    private fun score(
        timestampMs: Long,
        progressPercent: Int,
        checkpoint: Int,
        failureReason: GameplayFailureReason?,
    ): GameplayResult {
        val elapsedMs = (timestampMs - startedAtMs).coerceAtLeast(1L)
        val scoreReady = completedSteps >= GameplayScoringConfig.MIN_FORMAL_STEPS
        val speedScore = if (scoreReady) speedScore() else null
        val coordinationScore = if (scoreReady) coordinationScore() else 0
        val rhythmScore = rhythmScore()
        val diagnosticTimestampMs = diagnosticTimestampMs()
        if (failureReason != null) {
            return GameplayResult(
                elapsedMs = elapsedMs,
                progressPercent = progressPercent,
                checkpoint = checkpoint,
                completedSteps = completedSteps,
                eventCount = inputEventCount,
                speedScore = 0,
                coordinationScore = 0,
                highlight = "本局因摔倒失败",
                suggestion = "请重新站稳后再来一局",
                diagnosticTimestampMs = null,
                failureReason = failureReason,
            )
        }
        val highlight = when {
            !scoreReady -> "有效步数还不足，先完成几步再评价"
            coordinationScore >= 75 -> "换腿节奏稳定，配合很清楚"
            speedScore != null && speedScore >= 75 -> "推进积极，已经形成有效配合"
            else -> "完成了有效的四腿输入"
        }
        val suggestion = when {
            !scoreReady -> "再完成 ${GameplayScoringConfig.MIN_FORMAL_STEPS - completedSteps} 个有效步，解锁正式评分"
            rhythmScore < 60.0 ->
                "尝试让相邻有效步的间隔更接近（关注约 ${diagnosticTimestampMs ?: 0L} ms 的步）"
            completedSteps < GameplayScoringConfig.STEP_TARGET ->
                "保持当前节奏，再完成几步让推进更连贯"
            else -> "保持当前节奏，尝试减少停顿"
        }
        return GameplayResult(
            elapsedMs = elapsedMs,
            progressPercent = progressPercent,
            checkpoint = checkpoint,
            completedSteps = completedSteps,
            eventCount = inputEventCount,
            speedScore = speedScore,
            coordinationScore = coordinationScore,
            highlight = highlight,
            suggestion = suggestion,
            diagnosticTimestampMs = diagnosticTimestampMs,
        )
    }

    private fun speedScore(): Int {
        val cadence = stepIntervals().map(::cadenceScore).average()
        val continuity = continuityScore()
        val count = (completedSteps.toDouble() / GameplayScoringConfig.STEP_TARGET)
            .coerceAtMost(1.0) * 100.0
        return (cadence * 0.70 + continuity * 0.20 + count * 0.10)
            .roundToInt()
            .coerceIn(0, 100)
    }

    private fun coordinationScore(): Int {
        if (attemptedCount == 0) return 0
        val completion = completedSteps.toDouble() / attemptedCount
        val acceptance = acceptedCount.toDouble() / attemptedCount
        val rhythm = rhythmScore() / 100.0
        val participation = participationScore() / 100.0
        return (completion * 40.0 + acceptance * 25.0 + rhythm * 20.0 +
            participation * 15.0).roundToInt().coerceIn(0, 100)
    }

    private fun cadenceScore(intervalMs: Double): Double = when {
        intervalMs <= GameplayScoringConfig.FAST_STEP_MS -> 100.0
        intervalMs >= GameplayScoringConfig.SLOW_STEP_MS -> 10.0
        intervalMs <= 1_500.0 -> interpolate(intervalMs, 900.0, 1_500.0, 100.0, 75.0)
        intervalMs <= 2_200.0 -> interpolate(intervalMs, 1_500.0, 2_200.0, 75.0, 45.0)
        else -> interpolate(intervalMs, 2_200.0, 3_000.0, 45.0, 10.0)
    }

    private fun continuityScore(): Double {
        val longest = stepIntervals().maxOrNull() ?: return 0.0
        return when {
            longest <= GameplayScoringConfig.CONTINUITY_FULL_MS -> 100.0
            longest >= GameplayScoringConfig.CONTINUITY_ZERO_MS -> 0.0
            else -> interpolate(
                longest,
                GameplayScoringConfig.CONTINUITY_FULL_MS.toDouble(),
                GameplayScoringConfig.CONTINUITY_ZERO_MS.toDouble(),
                100.0,
                0.0,
            )
        }
    }

    private fun rhythmScore(): Double {
        val intervals = stepIntervals()
        if (intervals.size < 2) return 0.0
        val median = median(intervals).coerceAtLeast(1.0)
        val mad = median(intervals.map { abs(it - median) })
        return (1.0 - mad / (median * GameplayScoringConfig.RHYTHM_FULL_MAD_RATIO))
            .coerceIn(0.0, 1.0) * 100.0
    }

    private fun participationScore(): Double {
        val participatingLegs = completedStepLegs.distinct().size
        return participatingLegs.toDouble() / 4.0 * 100.0
    }

    private fun stepIntervals(): List<Double> = completedStepTimes
        .zipWithNext { first, second -> (second - first).coerceAtLeast(1L).toDouble() }

    private fun median(values: List<Double>): Double {
        if (values.isEmpty()) return 0.0
        val sorted = values.sorted()
        val middle = sorted.size / 2
        return if (sorted.size % 2 == 0) {
            (sorted[middle - 1] + sorted[middle]) / 2.0
        } else {
            sorted[middle]
        }
    }

    private fun interpolate(value: Double, low: Double, high: Double, lowScore: Double, highScore: Double): Double {
        val ratio = ((value - low) / (high - low)).coerceIn(0.0, 1.0)
        return lowScore + (highScore - lowScore) * ratio
    }

    private fun diagnosticTimestampMs(): Long? {
        val timestamps = completedStepTimes
        if (timestamps.isEmpty()) return null
        if (timestamps.size == 1) return timestamps.first() - startedAtMs
        val intervals = timestamps.zipWithNext { a, b -> b - a }
        val average = intervals.average()
        val index = intervals.indices.maxByOrNull { index ->
            kotlin.math.abs(intervals[index] - average)
        } ?: return null
        return timestamps[index + 1] - startedAtMs
    }

    private fun expirePendingAttempts(timestampMs: Long) {
        legAttempts.forEach { attempt ->
            if (attempt.requested && timestampMs - attempt.startedAtMs >
                GameplayScoringConfig.ATTEMPT_TIMEOUT_MS
            ) {
                attempt.reset()
            }
        }
    }

    private fun completeStep(index: Int, timestampMs: Long) {
        val attempt = legAttempts[index]
        if (!attempt.requested || !attempt.accepted || !attempt.released) return
        if (lastCompletedAtMs[index] != Long.MIN_VALUE &&
            timestampMs - lastCompletedAtMs[index] < GameplayScoringConfig.CONTACT_DEBOUNCE_MS
        ) {
            attempt.reset()
            return
        }
        completedStepTimes += timestampMs
        completedStepLegs += index
        lastCompletedAtMs[index] = timestampMs
        attempt.reset()
    }

    private fun telemetryFor(eventCount: Int, mask: Int): MockTelemetry {
        val progress = (completedSteps * 100 / GameplayScoringConfig.STEP_TARGET)
            .coerceAtMost(100)
        return MockTelemetry(
            state = if (mask == 0) "MOCK_SUPPORT" else "MOCK_HOLD",
            contact = mask == 0,
            progressPercent = progress,
            checkpoint = progress / 25,
        )
    }
}
