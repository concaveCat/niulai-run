package io.niulai.controller.protocol

import java.util.UUID

enum class Leg(val bit: Int, val shortLabel: String, val displayName: String) {
    FRONT_LEFT(0x01, "FL", "FRONT LEFT"),
    FRONT_RIGHT(0x02, "FR", "FRONT RIGHT"),
    REAR_LEFT(0x04, "RL", "REAR LEFT"),
    REAR_RIGHT(0x08, "RR", "REAR RIGHT"),
}

object NiulaiGatt {
    val SERVICE_UUID: UUID = UUID.fromString("7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11")
    val CONTROL_STATE_UUID: UUID = UUID.fromString("7d4e1001-8a52-4d9e-b0f8-57e6c59e2e11")
    val STATUS_UUID: UUID = UUID.fromString("7d4e1002-8a52-4d9e-b0f8-57e6c59e2e11")
    val HOME_COMMAND_UUID: UUID = UUID.fromString("7d4e1003-8a52-4d9e-b0f8-57e6c59e2e11")
    val IMU_TELEMETRY_UUID: UUID = UUID.fromString("7d4e1004-8a52-4d9e-b0f8-57e6c59e2e11")
    val GAME_MODE_UUID: UUID = UUID.fromString("7d4e1005-8a52-4d9e-b0f8-57e6c59e2e11")
    val AUDIO_COMMAND_UUID: UUID = UUID.fromString("7d4e1006-8a52-4d9e-b0f8-57e6c59e2e11")
    val CLIENT_CONFIGURATION_UUID: UUID =
        UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}

object BleProtocol {
    const val VERSION = 0x01
    const val PACKET_SIZE = 4
    const val HOME_PACKET_SIZE = 2
    const val AUDIO_PACKET_SIZE = 2
    const val TELEMETRY_PACKET_SIZE = 8
    const val HOME_COMMAND = 0x01
    const val AUDIO_COMMAND_PLAY_MAMA = 0x01
    const val LEG_MASK = 0x0f
    const val STATUS_FAILSAFE_ACTIVE = 0x01
    const val STATUS_FALL_DETECTED = 0x20
    const val STATUS_HOME_COMPLETE = 0x40
    // Android consumes semantic FL/FR/RL/RR bits. Physical GPIO mapping stays
    // in firmware; current defaults are GPIO40 -> FL, 39 -> FR, 42 -> RL, 41 -> RR.
    // STATUS flags bits 1..4 carry the debounced KW10 contact mask:
    // FL/FR/RL/RR map to bits 1/2/3/4 respectively.
    const val STATUS_CONTACT_MASK = 0x1e
    const val STATUS_CONTACT_MASK_SHIFT = 1
    const val STATUS_FLAGS_MASK = STATUS_FAILSAFE_ACTIVE or STATUS_CONTACT_MASK or
        STATUS_FALL_DETECTED or STATUS_HOME_COMPLETE
    const val TELEMETRY_VALID = 0x01
    const val TELEMETRY_CLIPPED = 0x02
    const val TELEMETRY_STALE = 0x04
    const val TELEMETRY_FLAGS_MASK = TELEMETRY_VALID or TELEMETRY_CLIPPED or TELEMETRY_STALE
    const val GAME_MODE_SOLO = 0x00
    const val GAME_MODE_MULTI = 0x01

    fun encodeControl(sequence: Int, legMask: Int): ByteArray {
        requireByte("sequence", sequence)
        require((legMask and LEG_MASK.inv()) == 0) { "leg mask contains reserved bits" }
        return byteArrayOf(VERSION.toByte(), sequence.toByte(), legMask.toByte(), 0x00)
    }

    fun decodeControl(bytes: ByteArray): ControlPacket {
        requirePacket(bytes)
        val version = unsigned(bytes[0])
        require(version == VERSION) { "unsupported protocol version: $version" }
        val legMask = unsigned(bytes[2])
        require((legMask and LEG_MASK.inv()) == 0) { "leg mask contains reserved bits" }
        require(unsigned(bytes[3]) == 0) { "control flags must be zero" }
        return ControlPacket(unsigned(bytes[1]), legMask)
    }

    fun decodeStatus(bytes: ByteArray): StatusPacket {
        requirePacket(bytes)
        val version = unsigned(bytes[0])
        require(version == VERSION) { "unsupported protocol version: $version" }
        val legMask = unsigned(bytes[2])
        val flags = unsigned(bytes[3])
        require((legMask and LEG_MASK.inv()) == 0) { "applied mask contains reserved bits" }
        require((flags and STATUS_FLAGS_MASK.inv()) == 0) { "status contains reserved flags" }
        return StatusPacket(unsigned(bytes[1]), legMask, flags)
    }

    fun encodeHomeCommand(): ByteArray = byteArrayOf(VERSION.toByte(), HOME_COMMAND.toByte())

    fun encodePlayMamaCommand(): ByteArray =
        byteArrayOf(VERSION.toByte(), AUDIO_COMMAND_PLAY_MAMA.toByte())

    fun encodeGameMode(mode: Int): ByteArray {
        require(mode == GAME_MODE_SOLO || mode == GAME_MODE_MULTI) {
            "unsupported game mode: $mode"
        }
        return byteArrayOf(VERSION.toByte(), mode.toByte())
    }

    fun decodeHomeCommand(bytes: ByteArray) {
        require(bytes.size == HOME_PACKET_SIZE) {
            "home packet must be exactly $HOME_PACKET_SIZE bytes"
        }
        require(unsigned(bytes[0]) == VERSION) {
            "unsupported protocol version: ${unsigned(bytes[0])}"
        }
        require(unsigned(bytes[1]) == HOME_COMMAND) {
            "unsupported home command: ${unsigned(bytes[1])}"
        }
    }

    fun decodeTelemetry(bytes: ByteArray): AccelerationTelemetry {
        require(bytes.size == TELEMETRY_PACKET_SIZE) {
            "telemetry packet must be exactly $TELEMETRY_PACKET_SIZE bytes"
        }
        require(unsigned(bytes[0]) == VERSION) {
            "unsupported protocol version: ${unsigned(bytes[0])}"
        }
        val flags = unsigned(bytes[1])
        require((flags and TELEMETRY_FLAGS_MASK.inv()) == 0) {
            "telemetry contains reserved flags"
        }
        val accelCentiG = unsigned(bytes[2]) or (unsigned(bytes[3]) shl 8)
        val ageMs = unsigned(bytes[4]) or (unsigned(bytes[5]) shl 8)
        val sequence = unsigned(bytes[6]) or (unsigned(bytes[7]) shl 8)
        return AccelerationTelemetry(
            valid = flags and TELEMETRY_VALID != 0,
            clipped = flags and TELEMETRY_CLIPPED != 0,
            stale = flags and TELEMETRY_STALE != 0,
            dynamicAccelerationG = accelCentiG / 100f,
            ageMs = ageMs,
            sequence = sequence,
        )
    }

    fun format(bytes: ByteArray): String = bytes.joinToString(" ") { "%02X".format(unsigned(it)) }

    private fun requirePacket(bytes: ByteArray) {
        require(bytes.size == PACKET_SIZE) { "packet must be exactly $PACKET_SIZE bytes" }
    }

    private fun requireByte(name: String, value: Int) {
        require(value in 0..0xff) { "$name must fit uint8" }
    }

    private fun unsigned(value: Byte): Int = value.toInt() and 0xff
}

data class ControlPacket(val sequence: Int, val legMask: Int) {
    fun encode(): ByteArray = BleProtocol.encodeControl(sequence, legMask)
}

data class StatusPacket(
    val sequence: Int,
    val appliedLegMask: Int,
    val flags: Int,
) {
    val failsafeActive: Boolean
        get() = flags and BleProtocol.STATUS_FAILSAFE_ACTIVE != 0

    val contactMask: Int
        get() = (flags and BleProtocol.STATUS_CONTACT_MASK) ushr
            BleProtocol.STATUS_CONTACT_MASK_SHIFT

    val fallDetected: Boolean
        get() = flags and BleProtocol.STATUS_FALL_DETECTED != 0

    val homeComplete: Boolean
        get() = flags and BleProtocol.STATUS_HOME_COMPLETE != 0
}

data class AccelerationTelemetry(
    val valid: Boolean,
    val clipped: Boolean,
    val stale: Boolean,
    val dynamicAccelerationG: Float,
    val ageMs: Int,
    val sequence: Int,
)
