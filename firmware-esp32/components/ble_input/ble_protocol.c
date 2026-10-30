#include "ble_protocol.h"

bool qw_ble_validate_control_packet(const uint8_t *data, size_t len,
                                    uint8_t *seq_out, uint8_t *mask_out)
{
    if (data == NULL || len != QW_BLE_PACKET_LEN) {
        return false;
    }
    if (data[0] != QW_BLE_VERSION) {
        return false;
    }
    if ((data[2] & QW_BLE_MASK_RESERVED) != 0) {
        return false;
    }
    if (data[3] != QW_BLE_FLAGS_NONE) {
        return false;
    }
    if (seq_out != NULL) {
        *seq_out = data[1];
    }
    if (mask_out != NULL) {
        *mask_out = data[2];
    }
    return true;
}

void qw_ble_build_status_packet(uint8_t out[QW_BLE_PACKET_LEN],
                                uint8_t last_seq, uint8_t applied_mask,
                                uint8_t flags)
{
    out[0] = QW_BLE_VERSION;
    out[1] = last_seq;
    out[2] = applied_mask;
    out[3] = flags;
}

bool qw_ble_validate_home_packet(const uint8_t *data, size_t len)
{
    return data != NULL && len == QW_BLE_HOME_PACKET_LEN &&
           data[0] == QW_BLE_VERSION && data[1] == QW_BLE_HOME_COMMAND;
}

bool qw_ble_validate_audio_command(const uint8_t *data, size_t len)
{
    return data != NULL && len == QW_BLE_AUDIO_PACKET_LEN &&
           data[0] == QW_BLE_VERSION &&
           data[1] == QW_BLE_AUDIO_COMMAND_PLAY_MAMA;
}

bool qw_ble_home_request_allowed(bool secure_link, bool neutral, bool busy)
{
    return secure_link && neutral && !busy;
}

bool qw_ble_home_latch_allowed(bool secure_link, bool busy)
{
    return secure_link && !busy;
}

bool qw_ble_control_request_allowed(bool secure_link, bool home_busy,
                                    bool neutral)
{
    /* A neutral CONTROL_STATE packet is allowed to complete an already
     * latched HOME request. Without this exception, a HOME sent during the
     * neutralization race could block the very packet needed to satisfy the
     * neutral-only execution gate. */
    return secure_link && (!home_busy || neutral);
}

void qw_ble_build_telemetry_packet(
    uint8_t out[QW_BLE_TELEMETRY_PACKET_LEN], uint8_t flags,
    uint16_t accel_centi_g, uint16_t age_ms, uint16_t sequence)
{
    if (out == NULL) return;
    out[0] = QW_BLE_VERSION;
    out[1] = flags & QW_BLE_TELEMETRY_FLAGS_MASK;
    out[2] = (uint8_t)(accel_centi_g & 0xffU);
    out[3] = (uint8_t)(accel_centi_g >> 8);
    out[4] = (uint8_t)(age_ms & 0xffU);
    out[5] = (uint8_t)(age_ms >> 8);
    out[6] = (uint8_t)(sequence & 0xffU);
    out[7] = (uint8_t)(sequence >> 8);
}
