package io.niulai.controller.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class BleProtocolTest {
    @Test
    fun encodesAndDecodesControlPacketAsExactlyFourBytes() {
        val encoded = BleProtocol.encodeControl(sequence = 52, legMask = 0x09)

        assertArrayEquals(byteArrayOf(0x01, 0x34, 0x09, 0x00), encoded)
        assertEquals(ControlPacket(sequence = 52, legMask = 0x09), BleProtocol.decodeControl(encoded))
    }

    @Test
    fun decodesStatusAndFailsafeFlag() {
        val normal = BleProtocol.decodeStatus(byteArrayOf(0x01, 0xff.toByte(), 0x08, 0x0a))
        val failsafe = BleProtocol.decodeStatus(byteArrayOf(0x01, 0x00, 0x00, 0x01))
        val fallen = BleProtocol.decodeStatus(byteArrayOf(0x01, 0x00, 0x00, 0x20))
        val home = BleProtocol.decodeStatus(byteArrayOf(0x01, 0x00, 0x00, 0x40))

        assertEquals(255, normal.sequence)
        assertEquals(0x08, normal.appliedLegMask)
        assertFalse(normal.failsafeActive)
        assertEquals(0x05, normal.contactMask)
        assertTrue(failsafe.failsafeActive)
        assertEquals(0x00, failsafe.contactMask)
        assertTrue(fallen.fallDetected)
        assertTrue(home.homeComplete)
    }

    @Test
    fun rejectsWrongLengthVersionAndReservedBits() {
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeControl(byteArrayOf(0x01, 0x00, 0x00))
        }
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeControl(byteArrayOf(0x02, 0x00, 0x00, 0x00))
        }
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeControl(byteArrayOf(0x01, 0x00, 0x10, 0x00))
        }
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeStatus(byteArrayOf(0x01, 0x00, 0x00, 0x80.toByte()))
        }
    }

    @Test
    fun encodesHomeAndDecodesGravityRemovedTelemetry() {
        assertArrayEquals(byteArrayOf(0x01, 0x01), BleProtocol.encodeHomeCommand())
        assertArrayEquals(byteArrayOf(0x01, 0x01), BleProtocol.encodePlayMamaCommand())
        BleProtocol.decodeHomeCommand(byteArrayOf(0x01, 0x01))
        val telemetry = BleProtocol.decodeTelemetry(
            byteArrayOf(0x01, 0x01, 0x2d, 0x00, 0x14, 0x00, 0x07, 0x00),
        )
        assertTrue(telemetry.valid)
        assertFalse(telemetry.clipped)
        assertEquals(0.45f, telemetry.dynamicAccelerationG)
        assertEquals(20, telemetry.ageMs)
        assertEquals(7, telemetry.sequence)
    }

    @Test
    fun encodesDisplayOnlyGameModeWithoutChangingControlPacket() {
        assertArrayEquals(byteArrayOf(0x01, 0x00), BleProtocol.encodeGameMode(BleProtocol.GAME_MODE_SOLO))
        assertArrayEquals(byteArrayOf(0x01, 0x01), BleProtocol.encodeGameMode(BleProtocol.GAME_MODE_MULTI))
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.encodeGameMode(9)
        }
    }

    @Test
    fun rejectsInvalidHomeAndTelemetryPackets() {
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeHomeCommand(byteArrayOf(0x01, 0x02))
        }
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeTelemetry(ByteArray(7))
        }
        assertThrows(IllegalArgumentException::class.java) {
            BleProtocol.decodeTelemetry(
                byteArrayOf(0x01, 0x80.toByte(), 0, 0, 0, 0, 0, 0),
            )
        }
    }
}
