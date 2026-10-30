/* Pure C11 codec for the Niulai BLE GATT Protocol v1
 * (docs/BLE_PROTOCOL.md, Accepted). Host-testable with plain gcc - no
 * NimBLE/ESP-IDF includes here; ble_gatt.c layers the NimBLE server on top.
 *
 * Packets are exactly 4 bytes:
 *   CONTROL_PACKET_V1: [ver=0x01][seq][mask][flags=0], mask bits 0..3 =
 *   FL/FR/RL/RR, bits 4..7 reserved (must be zero).
 *   STATUS_PACKET_V1:  [ver=0x01][last_seq][applied_mask][flags],
 *   flag bit 0 = FAILSAFE_ACTIVE, bits 1..4 = FL/FR/RL/RR contact mask,
 *   bit 5 = FALL_DETECTED; bit 6 = HOME_COMPLETE.
 *   HOME_COMMAND_V1:   [ver=0x01][command=0x01].
 *   IMU_TELEMETRY_V1:  [ver][flags][dynamic_accel_centi_g LE][age_ms LE]
 *                      [telemetry_sequence LE].
 *   AUDIO_COMMAND_V1:  [ver=0x01][command=PLAY_MAMA].
 */
#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QW_BLE_PACKET_LEN 4
#define QW_BLE_VERSION 0x01
#define QW_BLE_FLAGS_NONE 0x00

#define QW_BLE_HOME_PACKET_LEN 2
#define QW_BLE_HOME_COMMAND 0x01

#define QW_BLE_TELEMETRY_PACKET_LEN 8
#define QW_BLE_TELEMETRY_FLAG_VALID 0x01
#define QW_BLE_TELEMETRY_FLAG_CLIPPED 0x02
#define QW_BLE_TELEMETRY_FLAG_STALE 0x04
#define QW_BLE_TELEMETRY_FLAGS_MASK \
    (QW_BLE_TELEMETRY_FLAG_VALID | QW_BLE_TELEMETRY_FLAG_CLIPPED | \
     QW_BLE_TELEMETRY_FLAG_STALE)

#define QW_BLE_GAME_MODE_PACKET_LEN 2
#define QW_BLE_GAME_MODE_SOLO 0x00
#define QW_BLE_GAME_MODE_MULTI 0x01

#define QW_BLE_AUDIO_PACKET_LEN 2
#define QW_BLE_AUDIO_COMMAND_PLAY_MAMA 0x01

#define QW_BLE_MASK_FL 0x01
#define QW_BLE_MASK_FR 0x02
#define QW_BLE_MASK_RL 0x04
#define QW_BLE_MASK_RR 0x08
#define QW_BLE_MASK_RESERVED 0xF0

#define QW_BLE_STATUS_FLAG_FAILSAFE_ACTIVE 0x01
#define QW_BLE_STATUS_FLAG_CONTACT_MASK 0x1E
#define QW_BLE_STATUS_FLAG_FALL_DETECTED 0x20
#define QW_BLE_STATUS_FLAG_HOME_COMPLETE 0x40
#define QW_BLE_STATUS_CONTACT_MASK_SHIFT 1
#define QW_BLE_STATUS_FLAGS_MASK \
    (QW_BLE_STATUS_FLAG_FAILSAFE_ACTIVE | QW_BLE_STATUS_FLAG_CONTACT_MASK | \
     QW_BLE_STATUS_FLAG_FALL_DETECTED | QW_BLE_STATUS_FLAG_HOME_COMPLETE)

/* On-air (little-endian) byte sequences of the 128-bit UUIDs, byte-reversed
 * from the canonical strings in docs/BLE_PROTOCOL.md:
 *   service       7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11
 *   CONTROL_STATE 7d4e1001-8a52-4d9e-b0f8-57e6c59e2e11 (encrypted bonded WRITE)
 *   STATUS        7d4e1002-8a52-4d9e-b0f8-57e6c59e2e11 (READ | NOTIFY)
 * ble_gatt.c wraps these in ble_uuid128_t; tests/host/test_ble_protocol.c
 * reverses them back and compares against the canonical strings to guard
 * against byte-order regressions (this has broken before). */
#define QW_BLE_SVC_UUID_LE_BYTES                                              \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x00, 0x10, 0x4e, 0x7d}
#define QW_BLE_CONTROL_CHR_UUID_LE_BYTES                                      \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x01, 0x10, 0x4e, 0x7d}
#define QW_BLE_STATUS_CHR_UUID_LE_BYTES                                       \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x02, 0x10, 0x4e, 0x7d}
#define QW_BLE_HOME_CHR_UUID_LE_BYTES                                         \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x03, 0x10, 0x4e, 0x7d}
#define QW_BLE_TELEMETRY_CHR_UUID_LE_BYTES                                    \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x04, 0x10, 0x4e, 0x7d}
#define QW_BLE_GAME_MODE_CHR_UUID_LE_BYTES                                    \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x05, 0x10, 0x4e, 0x7d}
#define QW_BLE_AUDIO_CHR_UUID_LE_BYTES                                        \
    {0x11, 0x2e, 0x9e, 0xc5, 0xe6, 0x57, 0xf8, 0xb0, 0x9e, 0x4d, 0x52, 0x8a,  \
     0x06, 0x10, 0x4e, 0x7d}

/* Validates a CONTROL_PACKET_V1: exactly 4 bytes, version 0x01, reserved
 * mask bits (4..7) zero, flags zero. On success writes seq/mask to the
 * optional out params (NULL tolerated) and returns true; on failure returns
 * false and leaves the out params untouched. */
bool qw_ble_validate_control_packet(const uint8_t *data, size_t len,
                                    uint8_t *seq_out, uint8_t *mask_out);

/* Builds a STATUS_PACKET_V1 into out[4] from the latest accepted sequence,
 * the currently applied mask and the status flags. */
void qw_ble_build_status_packet(uint8_t out[QW_BLE_PACKET_LEN],
                                uint8_t last_seq, uint8_t applied_mask,
                                uint8_t flags);

bool qw_ble_validate_home_packet(const uint8_t *data, size_t len);

bool qw_ble_validate_audio_command(const uint8_t *data, size_t len);

/* Pure admission gates kept beside the packet codec so host tests can cover
 * the same combinations used by the ESP32 state adapter. HOME is latched
 * after authentication and consumed only after a neutral packet is observed. */
bool qw_ble_home_request_allowed(bool secure_link, bool neutral, bool busy);
bool qw_ble_home_latch_allowed(bool secure_link, bool busy);
bool qw_ble_control_request_allowed(bool secure_link, bool home_busy,
                                    bool neutral);

void qw_ble_build_telemetry_packet(
    uint8_t out[QW_BLE_TELEMETRY_PACKET_LEN], uint8_t flags,
    uint16_t accel_centi_g, uint16_t age_ms, uint16_t sequence);

#ifdef __cplusplus
}
#endif

#endif /* BLE_PROTOCOL_H */
