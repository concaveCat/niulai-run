package io.niulai.controller.control

import io.niulai.controller.protocol.ControlPacket
import io.niulai.controller.protocol.Leg

/** Pure state holder for multi-pointer hold controls. */
class LegInputTracker {
    private val activePointers = mutableMapOf<Long, Leg>()

    var sequence: Int = 0
        private set

    val legMask: Int
        get() = activePointers.values.fold(0) { mask, leg -> mask or leg.bit }

    val packet: ControlPacket
        get() = ControlPacket(sequence, legMask)

    fun pointerDown(pointerId: Long, leg: Leg): ControlPacket? {
        if (activePointers[pointerId] == leg) return null
        activePointers[pointerId] = leg
        advanceSequence()
        return packet
    }

    fun pointerUp(pointerId: Long): ControlPacket? {
        if (activePointers.remove(pointerId) == null) return null
        advanceSequence()
        return packet
    }

    fun cancelLeg(leg: Leg): ControlPacket? {
        val removed = activePointers.entries.removeAll { it.value == leg }
        if (!removed) return null
        advanceSequence()
        return packet
    }

    /**
     * Clears all pointers. When [forcePacket] is true, advances the sequence even
     * if already neutral so a lifecycle/disconnect zero is an explicit packet.
     */
    fun clear(forcePacket: Boolean): ControlPacket? {
        if (activePointers.isEmpty() && !forcePacket) return null
        activePointers.clear()
        advanceSequence()
        return packet
    }

    private fun advanceSequence() {
        sequence = (sequence + 1) and 0xff
    }
}
