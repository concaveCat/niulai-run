package io.niulai.controller.gameplay

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class GameplaySessionTest {
    @Test
    fun recordsOnlyChangedMasksWithMockTelemetry() {
        val session = GameplaySession(GameplayMode.SPLIT, startedAtMs = 1_000L)

        session.recordMask(timestampMs = 2_000L, legMask = 0x01)
        session.recordMask(timestampMs = 2_500L, legMask = 0x01)
        session.recordMask(timestampMs = 3_000L, legMask = 0x00)

        assertEquals(3, session.events.size)
        assertEquals(GameplayEventKind.SESSION_START, session.events[0].kind)
        assertEquals(0x01, session.events[1].changedMask)
        assertEquals("MOCK_HOLD", session.events[1].telemetry.state)
        assertEquals("MOCK_SUPPORT", session.events[2].telemetry.state)
        assertTrue(session.events[2].telemetry.contact)
    }

    @Test
    fun sameEventTimelineProducesSameScores() {
        val first = sessionWithTimeline()
        val second = sessionWithTimeline()

        assertEquals(first.finish(5_000L), second.finish(5_000L))
        assertEquals(first.events, second.events)
    }

    @Test
    fun noProgressDoesNotInventSpeedScore() {
        val result = GameplaySession(GameplayMode.SOLO, 1_000L).finish(2_000L)

        assertNull(result.speedScore)
        assertEquals(0, result.progressPercent)
        assertEquals(0, result.completedSteps)
        assertEquals(0, result.eventCount)
    }

    @Test
    fun oneSimulatedPressReleaseIsProvisionalNotAFormalScore() {
        val session = GameplaySession(GameplayMode.SOLO, 1_000L)
        session.recordMask(1_500L, 0x01)
        session.recordMask(2_000L, 0x00)

        val result = session.finish(3_000L)

        assertEquals(1, result.completedSteps)
        assertEquals(12, result.progressPercent)
        assertNull(result.speedScore)
        assertEquals(0, result.coordinationScore)
        assertTrue(!result.scoreReady)
    }

    @Test
    fun entityStepRequiresAppliedDepartureReleaseAndContactReturn() {
        val session = GameplaySession(GameplayMode.SOLO, 1_000L)
        session.recordStatus(1_000L, appliedLegMask = 0x00, contactMask = 0x01)
        session.recordMask(1_100L, 0x01, simulateStep = false)
        session.recordStatus(1_200L, appliedLegMask = 0x01, contactMask = 0x01)
        session.recordStatus(1_300L, appliedLegMask = 0x01, contactMask = 0x00)
        session.recordMask(1_400L, 0x00, simulateStep = false)
        session.recordStatus(1_500L, appliedLegMask = 0x00, contactMask = 0x00)
        session.recordStatus(1_700L, appliedLegMask = 0x00, contactMask = 0x01)

        val result = session.finish(2_000L)

        assertEquals(1, result.completedSteps)
        assertEquals(12, result.progressPercent)
        assertNull(result.speedScore)
        assertEquals(0, result.coordinationScore)
    }

    @Test
    fun rejectedEntityInputDoesNotBecomeACompletedStep() {
        val session = GameplaySession(GameplayMode.SOLO, 1_000L)
        session.recordStatus(1_000L, appliedLegMask = 0x00, contactMask = 0x01)
        session.recordMask(1_100L, 0x01, simulateStep = false)
        session.recordMask(1_400L, 0x00, simulateStep = false)
        session.recordStatus(1_500L, appliedLegMask = 0x00, contactMask = 0x01)

        val result = session.finish(2_000L)

        assertEquals(0, result.completedSteps)
        assertNull(result.speedScore)
        assertEquals(0, result.coordinationScore)
    }

    @Test
    fun feedbackSummaryContainsOnlyAggregateLegPressCounts() {
        val session = GameplaySession(GameplayMode.SPLIT, 1_000L).also {
            it.recordMask(2_000L, 0x01)
            it.recordMask(2_500L, 0x03)
            it.recordMask(3_000L, 0x00)
        }

        val result = session.finish(5_000L)
        val summary = session.feedbackSummary(result)

        assertEquals(listOf(1, 1, 0, 0), summary.legPressCounts)
        assertEquals(3, summary.eventCount)
        assertEquals(GameplayMode.SPLIT, summary.mode)
        assertTrue(summary.rhythmConsistencyPercent in 0..100)
    }

    @Test
    fun fallFinishesWithZeroScoresAndFailureSummary() {
        val session = GameplaySession(GameplayMode.SOLO, 1_000L).also {
            it.recordMask(2_000L, 0x01)
        }

        val result = session.finish(3_000L, GameplayFailureReason.FALL)
        val summary = session.feedbackSummary(result)

        assertTrue(result.failed)
        assertEquals(GameplayFailureReason.FALL, result.failureReason)
        assertEquals(0, result.speedScore)
        assertEquals(0, result.coordinationScore)
        assertEquals(GameplayFailureReason.FALL, summary.failureReason)
        assertEquals("本局因摔倒失败", result.highlight)
    }

    @Test
    fun eventListIsSnapshotAndDoesNotLeakMutableState() {
        val session = sessionWithTimeline()
        val firstSnapshot = session.events
        val copied = firstSnapshot.toMutableList()
        copied.clear()

        assertNotEquals(0, session.events.size)
        assertEquals(firstSnapshot, session.events)
    }

    private fun sessionWithTimeline(): GameplaySession =
        GameplaySession(GameplayMode.SOLO, 1_000L).also { session ->
            session.recordMask(2_000L, 0x01)
            session.recordMask(3_000L, 0x00)
        }
}
