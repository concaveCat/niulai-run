package io.niulai.controller.control

import io.niulai.controller.protocol.Leg
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class LegInputTrackerTest {
    @Test
    fun producesRequiredCompleteMaskSequence() {
        val tracker = LegInputTracker()

        assertEquals(0x00, tracker.legMask)
        assertEquals(0x01, tracker.pointerDown(1, Leg.FRONT_LEFT)?.legMask)
        assertEquals(0x09, tracker.pointerDown(2, Leg.REAR_RIGHT)?.legMask)
        assertEquals(0x08, tracker.pointerUp(1)?.legMask)
        assertEquals(0x00, tracker.pointerUp(2)?.legMask)
        assertEquals(4, tracker.sequence)
    }

    @Test
    fun cancellationReleasesOnlyAffectedControl() {
        val tracker = LegInputTracker()
        tracker.pointerDown(10, Leg.FRONT_LEFT)
        tracker.pointerDown(11, Leg.REAR_RIGHT)

        assertEquals(0x08, tracker.cancelLeg(Leg.FRONT_LEFT)?.legMask)
        assertEquals(0x08, tracker.legMask)
        assertNull(tracker.pointerUp(10))
    }

    @Test
    fun aLegRemainsPressedUntilItsLastPointerIsReleased() {
        val tracker = LegInputTracker()
        tracker.pointerDown(1, Leg.FRONT_RIGHT)
        tracker.pointerDown(2, Leg.FRONT_RIGHT)

        assertEquals(0x02, tracker.pointerUp(1)?.legMask)
        assertEquals(0x00, tracker.pointerUp(2)?.legMask)
    }

    @Test
    fun disconnectStyleClearResetsLocalMask() {
        val tracker = LegInputTracker()
        tracker.pointerDown(1, Leg.REAR_LEFT)

        assertEquals(0x00, tracker.clear(forcePacket = false)?.legMask)
        assertEquals(0x00, tracker.legMask)
    }

    @Test
    fun explicitNeutralPacketAdvancesSequenceEvenWhenAlreadyClear() {
        val tracker = LegInputTracker()

        assertEquals(0x00, tracker.clear(forcePacket = true)?.legMask)
        assertEquals(1, tracker.sequence)
    }

    @Test
    fun sequenceWrapsNaturallyAt255() {
        val tracker = LegInputTracker()
        repeat(128) { index ->
            tracker.pointerDown(index.toLong(), Leg.FRONT_LEFT)
            tracker.pointerUp(index.toLong())
        }

        assertEquals(0, tracker.sequence)
        assertEquals(0, tracker.legMask)
    }
}
