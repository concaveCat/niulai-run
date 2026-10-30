/*
 * Host-only mock of the BLE Central -> GATT write -> remote input -> STATUS
 * path. It deliberately composes the same pure codec and input-state helpers
 * used by the ESP32 endpoint; no radio, NimBLE or ESP-IDF is involved.
 */
#include "ble_protocol.h"
#include "qw_check.h"
#include "remote_input_state.h"

#include <stdint.h>
#include <string.h>

#define MOCK_STATUS_HISTORY 32U
#define MOCK_WATCHDOG_TIMEOUT_US 500000

typedef struct {
    remote_leg_input_t input;
    bool link_secure;
    uint8_t contact_mask;
    uint8_t status_history[MOCK_STATUS_HISTORY][QW_BLE_PACKET_LEN];
    unsigned status_count;
    unsigned invalid_count;
} mock_peripheral_t;

static void mock_peripheral_init(mock_peripheral_t *peripheral)
{
    memset(peripheral, 0, sizeof(*peripheral));
    qw_remote_input_init(&peripheral->input);
    peripheral->link_secure = true;
}

static void mock_emit_status(mock_peripheral_t *peripheral)
{
    uint8_t flags = peripheral->input.failsafe_active
                        ? QW_BLE_STATUS_FLAG_FAILSAFE_ACTIVE : 0U;
    flags |= (uint8_t)(peripheral->contact_mask <<
                       QW_BLE_STATUS_CONTACT_MASK_SHIFT);
    if (peripheral->status_count < MOCK_STATUS_HISTORY) {
        qw_ble_build_status_packet(
            peripheral->status_history[peripheral->status_count],
            peripheral->input.sequence, peripheral->input.leg_mask,
            flags & QW_BLE_STATUS_FLAGS_MASK);
    }
    peripheral->status_count++;
}

static const uint8_t *mock_last_status(const mock_peripheral_t *peripheral)
{
    CHECK(peripheral->status_count > 0U);
    unsigned index = peripheral->status_count - 1U;
    if (index >= MOCK_STATUS_HISTORY) index = MOCK_STATUS_HISTORY - 1U;
    return peripheral->status_history[index];
}

static void central_encode_control(uint8_t out[QW_BLE_PACKET_LEN],
                                   uint8_t sequence, uint8_t leg_mask)
{
    /* This is the byte layout emitted by Android BleProtocol.encodeControl. */
    out[0] = QW_BLE_VERSION;
    out[1] = sequence;
    out[2] = leg_mask;
    out[3] = QW_BLE_FLAGS_NONE;
}

static bool mock_write_control(mock_peripheral_t *peripheral,
                               const uint8_t *packet, size_t length,
                               int64_t now_us)
{
    uint8_t sequence = 0;
    uint8_t leg_mask = 0;
    if (!peripheral->link_secure) {
        peripheral->invalid_count++;
        return false;
    }
    if (!qw_ble_validate_control_packet(packet, length, &sequence,
                                        &leg_mask)) {
        peripheral->invalid_count++;
        return false;
    }
    qw_remote_input_accept(&peripheral->input, sequence, leg_mask, now_us);
    mock_emit_status(peripheral);
    return true;
}

static bool mock_set_contact_mask(mock_peripheral_t *peripheral,
                                  uint8_t contact_mask)
{
    contact_mask &= (uint8_t)~QW_BLE_MASK_RESERVED;
    if (contact_mask == peripheral->contact_mask) return false;
    peripheral->contact_mask = contact_mask;
    mock_emit_status(peripheral);
    return true;
}

static bool mock_watchdog(mock_peripheral_t *peripheral, int64_t now_us)
{
    bool changed = qw_remote_input_apply_failsafe(
        &peripheral->input, now_us, MOCK_WATCHDOG_TIMEOUT_US);
    if (changed) mock_emit_status(peripheral);
    return changed;
}

static void mock_disconnect(mock_peripheral_t *peripheral)
{
    qw_remote_input_clear(&peripheral->input);
}

static void mock_read_status(const mock_peripheral_t *peripheral,
                             uint8_t out[QW_BLE_PACKET_LEN])
{
    uint8_t flags = peripheral->input.failsafe_active
                        ? QW_BLE_STATUS_FLAG_FAILSAFE_ACTIVE : 0U;
    flags |= (uint8_t)(peripheral->contact_mask <<
                       QW_BLE_STATUS_CONTACT_MASK_SHIFT);
    qw_ble_build_status_packet(out, peripheral->input.sequence,
                               peripheral->input.leg_mask,
                               flags & QW_BLE_STATUS_FLAGS_MASK);
}

static void expect_status(const mock_peripheral_t *peripheral,
                          uint8_t sequence, uint8_t applied_mask,
                          uint8_t flags)
{
    const uint8_t *status = mock_last_status(peripheral);
    CHECK_INT_EQ(status[0], QW_BLE_VERSION);
    CHECK_INT_EQ(status[1], sequence);
    CHECK_INT_EQ(status[2], applied_mask);
    CHECK_INT_EQ(status[3], flags);
}

static void test_android_state_sequence_reaches_status(void)
{
    mock_peripheral_t peripheral;
    uint8_t packet[QW_BLE_PACKET_LEN];
    mock_peripheral_init(&peripheral);

    uint8_t initial_status[QW_BLE_PACKET_LEN];
    mock_read_status(&peripheral, initial_status);
    CHECK_INT_EQ(initial_status[0], QW_BLE_VERSION);
    CHECK_INT_EQ(initial_status[1], 0);
    CHECK_INT_EQ(initial_status[2], 0);
    CHECK_INT_EQ(initial_status[3], 0);

    central_encode_control(packet, 0, 0x00);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 1000000));
    expect_status(&peripheral, 0, 0x00, 0x00);

    central_encode_control(packet, 1, QW_BLE_MASK_FL);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 1100000));
    expect_status(&peripheral, 1, QW_BLE_MASK_FL, 0x00);

    /* Android's heartbeat repeats the complete packet and sequence. */
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 1200000));
    expect_status(&peripheral, 1, QW_BLE_MASK_FL, 0x00);

    CHECK(mock_set_contact_mask(&peripheral,
                                QW_BLE_MASK_FL | QW_BLE_MASK_RR));
    expect_status(&peripheral, 1, QW_BLE_MASK_FL, 0x12);

    central_encode_control(packet, 2, QW_BLE_MASK_FL | QW_BLE_MASK_RR);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 2000000));
    expect_status(&peripheral, 2, 0x09, 0x12);
    CHECK_INT_EQ(peripheral.status_count, 5);
}

static void test_invalid_packet_is_not_applied(void)
{
    mock_peripheral_t peripheral;
    uint8_t packet[QW_BLE_PACKET_LEN];
    mock_peripheral_init(&peripheral);
    central_encode_control(packet, 7, QW_BLE_MASK_RL);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 3000000));

    packet[2] |= QW_BLE_MASK_RESERVED;
    CHECK(!mock_write_control(&peripheral, packet, sizeof(packet), 3000100));
    packet[2] = QW_BLE_MASK_RL;
    packet[3] = 1;
    CHECK(!mock_write_control(&peripheral, packet, sizeof(packet), 3000200));
    CHECK(!mock_write_control(&peripheral, packet, sizeof(packet) - 1U,
                              3000300));
    CHECK_INT_EQ(peripheral.invalid_count, 3);
    CHECK_INT_EQ(peripheral.status_count, 1);
    CHECK_INT_EQ(peripheral.input.sequence, 7);
    CHECK_INT_EQ(peripheral.input.leg_mask, QW_BLE_MASK_RL);
    CHECK_INT_EQ(peripheral.input.last_rx_time_us, 3000000);
    expect_status(&peripheral, 7, QW_BLE_MASK_RL, 0x00);
}

static void test_unsecure_write_is_not_applied(void)
{
    mock_peripheral_t peripheral;
    uint8_t packet[QW_BLE_PACKET_LEN];
    mock_peripheral_init(&peripheral);
    peripheral.link_secure = false;
    central_encode_control(packet, 8, QW_BLE_MASK_FL);

    CHECK(!mock_write_control(&peripheral, packet, sizeof(packet), 3500000));
    CHECK_INT_EQ(peripheral.invalid_count, 1);
    CHECK_INT_EQ(peripheral.input.leg_mask, 0);
    CHECK_INT_EQ(peripheral.status_count, 0);
}

static void test_exact_watchdog_boundary_and_recovery(void)
{
    mock_peripheral_t peripheral;
    uint8_t packet[QW_BLE_PACKET_LEN];
    mock_peripheral_init(&peripheral);
    central_encode_control(packet, 20, QW_BLE_MASK_FR | QW_BLE_MASK_RL);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 4000000));

    CHECK(!mock_watchdog(&peripheral, 4499999));
    CHECK_INT_EQ(peripheral.input.leg_mask, 0x06);
    CHECK_INT_EQ(peripheral.status_count, 1);
    CHECK(mock_watchdog(&peripheral, 4500000));
    CHECK_INT_EQ(peripheral.input.leg_mask, 0x00);
    CHECK(peripheral.input.failsafe_active);
    expect_status(&peripheral, 20, 0x00, 0x01);

    central_encode_control(packet, 21, QW_BLE_MASK_RR);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 4500001));
    CHECK(!peripheral.input.failsafe_active);
    expect_status(&peripheral, 21, QW_BLE_MASK_RR, 0x00);
}

static void test_disconnect_clears_without_failsafe_status(void)
{
    mock_peripheral_t peripheral;
    uint8_t packet[QW_BLE_PACKET_LEN];
    uint8_t status[QW_BLE_PACKET_LEN];
    mock_peripheral_init(&peripheral);
    central_encode_control(packet, 99, QW_BLE_MASK_FL | QW_BLE_MASK_RR);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 5000000));
    CHECK(mock_set_contact_mask(&peripheral, QW_BLE_MASK_FR));
    unsigned notifications_before_disconnect = peripheral.status_count;

    mock_disconnect(&peripheral);
    CHECK_INT_EQ(peripheral.input.sequence, 99);
    CHECK_INT_EQ(peripheral.input.leg_mask, 0);
    CHECK(!peripheral.input.has_received);
    CHECK(!peripheral.input.failsafe_active);
    CHECK_INT_EQ(peripheral.status_count, notifications_before_disconnect);
    mock_read_status(&peripheral, status);
    CHECK_INT_EQ(status[1], 99);
    CHECK_INT_EQ(status[2], 0);
    CHECK_INT_EQ(status[3], 0x04);
}

static void test_sequence_wrap_is_observability_only(void)
{
    mock_peripheral_t peripheral;
    uint8_t packet[QW_BLE_PACKET_LEN];
    mock_peripheral_init(&peripheral);
    central_encode_control(packet, 0xff, QW_BLE_MASK_FL);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 6000000));
    central_encode_control(packet, 0x00, QW_BLE_MASK_RR);
    CHECK(mock_write_control(&peripheral, packet, sizeof(packet), 6000100));
    CHECK_INT_EQ(peripheral.input.sequence, 0);
    CHECK_INT_EQ(peripheral.input.leg_mask, QW_BLE_MASK_RR);
    expect_status(&peripheral, 0, QW_BLE_MASK_RR, 0x00);
}

int main(void)
{
    RUN_TEST(test_android_state_sequence_reaches_status);
    RUN_TEST(test_invalid_packet_is_not_applied);
    RUN_TEST(test_unsecure_write_is_not_applied);
    RUN_TEST(test_exact_watchdog_boundary_and_recovery);
    RUN_TEST(test_disconnect_clears_without_failsafe_status);
    RUN_TEST(test_sequence_wrap_is_observability_only);
    printf("test_ble_endpoint_mock: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
