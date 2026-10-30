/* Host tests for the pure-C BLE protocol codec (components/ble_input).
 * Covers the packet validation matrix and the STATUS packet layout from the
 * plan, plus a byte-order check on the 128-bit UUID literals (the on-air
 * byte order has broken in a previous team implementation, so the literal
 * byte sequences are reversed back and compared against the canonical
 * strings from docs/BLE_PROTOCOL.md). */
#include <stdint.h>

#include "qw_check.h"
#include "ble_protocol.h"

static void test_ble_packet_validation_matrix(void)
{
    uint8_t seq, mask;

    /* valid: version, sequence, all-four-legs mask, zero flags */
    CHECK(qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x34, 0x09,
                                                           0x00},
                                         4, &seq, &mask));
    CHECK_INT_EQ(seq, 0x34);
    CHECK_INT_EQ(mask, 0x09);

    /* valid: zero mask */
    CHECK(qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x00, 0x00,
                                                           0x00},
                                         4, &seq, &mask));
    CHECK_INT_EQ(seq, 0x00);
    CHECK_INT_EQ(mask, 0x00);

    /* valid: mask 0x0F (all four legs) */
    CHECK(qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x7F, 0x0F,
                                                           0x00},
                                         4, &seq, &mask));
    CHECK_INT_EQ(seq, 0x7F);
    CHECK_INT_EQ(mask, 0x0F);

    /* length must be exactly 4 */
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x00, 0x00},
                                          3, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x00, 0x00,
                                                            0x00, 0x00},
                                          5, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet(NULL, 4, &seq, &mask));

    /* version must be 0x01 */
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x00, 0x01, 0x00,
                                                            0x00},
                                          4, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x02, 0x01, 0x00,
                                                            0x00},
                                          4, &seq, &mask));

    /* reserved mask bits (4..7) must be zero */
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x10,
                                                            0x00},
                                          4, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x80,
                                                            0x00},
                                          4, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0xF0,
                                                            0x00},
                                          4, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x0F |
                                                                    0x10,
                                                            0x00},
                                          4, &seq, &mask));

    /* flags must be zero */
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x00,
                                                            0x01},
                                          4, &seq, &mask));
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x00,
                                                            0xFF},
                                          4, &seq, &mask));

    /* out params untouched on rejection */
    seq = 0xAA;
    mask = 0xAA;
    CHECK(!qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x10,
                                                            0x00},
                                          4, &seq, &mask));
    CHECK_INT_EQ(seq, 0xAA);
    CHECK_INT_EQ(mask, 0xAA);

    /* NULL out params tolerated on success */
    CHECK(qw_ble_validate_control_packet((const uint8_t[]){0x01, 0x01, 0x00,
                                                           0x00},
                                         4, NULL, NULL));
}

static void test_ble_status_packet_layout(void)
{
    uint8_t out[QW_BLE_PACKET_LEN] = {0xEE, 0xEE, 0xEE, 0xEE};

    qw_ble_build_status_packet(out, 0x34, 0x09, 0x01);
    CHECK_INT_EQ(out[0], 0x01); /* version */
    CHECK_INT_EQ(out[1], 0x34); /* last accepted sequence */
    CHECK_INT_EQ(out[2], 0x09); /* applied mask */
    CHECK_INT_EQ(out[3], 0x01); /* FAILSAFE_ACTIVE */

    qw_ble_build_status_packet(out, 0x00, 0x00, 0x00);
    CHECK_INT_EQ(out[0], 0x01);
    CHECK_INT_EQ(out[1], 0x00);
    CHECK_INT_EQ(out[2], 0x00);
    CHECK_INT_EQ(out[3], 0x00);

    /* sequence wrap plus failsafe and FL/RL contact flags */
    qw_ble_build_status_packet(
        out, 0xFF, 0x0F,
        QW_BLE_STATUS_FLAG_FAILSAFE_ACTIVE |
            (0x05u << QW_BLE_STATUS_CONTACT_MASK_SHIFT) |
            QW_BLE_STATUS_FLAG_FALL_DETECTED);
    CHECK_INT_EQ(out[0], 0x01);
    CHECK_INT_EQ(out[1], 0xFF);
    CHECK_INT_EQ(out[2], 0x0F);
    CHECK_INT_EQ(out[3], 0x2B);

    qw_ble_build_status_packet(out, 0x01, 0x00,
                               QW_BLE_STATUS_FLAG_HOME_COMPLETE);
    CHECK_INT_EQ(out[3], 0x40);
}

static void test_ble_home_and_telemetry_layout(void)
{
    uint8_t telemetry[QW_BLE_TELEMETRY_PACKET_LEN] = {0};
    CHECK(qw_ble_validate_home_packet((const uint8_t[]){QW_BLE_VERSION,
                                                         QW_BLE_HOME_COMMAND},
                                      QW_BLE_HOME_PACKET_LEN));
    CHECK(!qw_ble_validate_home_packet((const uint8_t[]){QW_BLE_VERSION, 0x02},
                                       QW_BLE_HOME_PACKET_LEN));
    CHECK(!qw_ble_validate_home_packet((const uint8_t[]){QW_BLE_VERSION}, 1));

    CHECK(qw_ble_validate_audio_command(
        (const uint8_t[]){QW_BLE_VERSION, QW_BLE_AUDIO_COMMAND_PLAY_MAMA},
        QW_BLE_AUDIO_PACKET_LEN));
    CHECK(!qw_ble_validate_audio_command(
        (const uint8_t[]){QW_BLE_VERSION, 0x02}, QW_BLE_AUDIO_PACKET_LEN));
    CHECK(!qw_ble_validate_audio_command(
        (const uint8_t[]){QW_BLE_VERSION}, 1));

    qw_ble_build_telemetry_packet(
        telemetry, QW_BLE_TELEMETRY_FLAG_VALID, 45U, 20U, 7U);
    CHECK_INT_EQ(telemetry[0], QW_BLE_VERSION);
    CHECK_INT_EQ(telemetry[1], QW_BLE_TELEMETRY_FLAG_VALID);
    CHECK_INT_EQ(telemetry[2], 45);
    CHECK_INT_EQ(telemetry[3], 0);
    CHECK_INT_EQ(telemetry[4], 20);
    CHECK_INT_EQ(telemetry[5], 0);
    CHECK_INT_EQ(telemetry[6], 7);
    CHECK_INT_EQ(telemetry[7], 0);
}

static void test_ble_admission_gates(void)
{
    CHECK(qw_ble_home_request_allowed(true, true, false));
    CHECK(!qw_ble_home_request_allowed(false, true, false));
    CHECK(!qw_ble_home_request_allowed(true, false, false));
    CHECK(!qw_ble_home_request_allowed(true, true, true));
    CHECK(qw_ble_home_latch_allowed(true, false));
    CHECK(!qw_ble_home_latch_allowed(false, false));
    CHECK(!qw_ble_home_latch_allowed(true, true));

    CHECK(qw_ble_control_request_allowed(true, false, false));
    CHECK(!qw_ble_control_request_allowed(false, false, false));
    CHECK(!qw_ble_control_request_allowed(true, true, false));
    CHECK(qw_ble_control_request_allowed(true, true, true));
}

/* canonical 128-bit UUID byte strings from docs/BLE_PROTOCOL.md */
static const uint8_t k_canon_svc[16] = {0x7d, 0x4e, 0x10, 0x00, 0x8a, 0x52,
                                        0x4d, 0x9e, 0xb0, 0xf8, 0x57, 0xe6,
                                        0xc5, 0x9e, 0x2e, 0x11};
static const uint8_t k_canon_control[16] = {0x7d, 0x4e, 0x10, 0x01, 0x8a,
                                            0x52, 0x4d, 0x9e, 0xb0, 0xf8,
                                            0x57, 0xe6, 0xc5, 0x9e, 0x2e,
                                            0x11};
static const uint8_t k_canon_status[16] = {0x7d, 0x4e, 0x10, 0x02, 0x8a,
                                           0x52, 0x4d, 0x9e, 0xb0, 0xf8,
                                            0x57, 0xe6, 0xc5, 0x9e, 0x2e,
                                            0x11};
static const uint8_t k_canon_audio[16] = {0x7d, 0x4e, 0x10, 0x06, 0x8a,
                                          0x52, 0x4d, 0x9e, 0xb0, 0xf8,
                                          0x57, 0xe6, 0xc5, 0x9e, 0x2e,
                                          0x11};

static void check_reverses_to(const uint8_t le[16], const uint8_t canonical[16])
{
    /* BLE_UUID128_INIT bytes are on-air order (little-endian): the reverse
     * of the literal must equal the canonical big-endian string. */
    for (int i = 0; i < 16; i++) {
        CHECK_INT_EQ(le[15 - i], canonical[i]);
    }
}

static void test_uuid_literals_are_byte_reversed(void)
{
    const uint8_t svc[16] = QW_BLE_SVC_UUID_LE_BYTES;
    const uint8_t control[16] = QW_BLE_CONTROL_CHR_UUID_LE_BYTES;
    const uint8_t status[16] = QW_BLE_STATUS_CHR_UUID_LE_BYTES;
    const uint8_t audio[16] = QW_BLE_AUDIO_CHR_UUID_LE_BYTES;
    check_reverses_to(svc, k_canon_svc);
    check_reverses_to(control, k_canon_control);
    check_reverses_to(status, k_canon_status);
    check_reverses_to(audio, k_canon_audio);
}

int main(void)
{
    RUN_TEST(test_ble_packet_validation_matrix);
    RUN_TEST(test_ble_status_packet_layout);
    RUN_TEST(test_ble_home_and_telemetry_layout);
    RUN_TEST(test_ble_admission_gates);
    RUN_TEST(test_uuid_literals_are_byte_reversed);
    printf("test_ble_protocol: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
