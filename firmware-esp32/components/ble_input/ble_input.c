/* Spinlock-guarded shared remote input state + 500 ms failsafe watchdog.
 * Single writer: the NimBLE write callback (accept) and the watchdog task
 * (watchdog_check). Readers: control task snapshot, GATT read callback. */
#include "sdkconfig.h"

#include "ble_input.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include <math.h>
#include <limits.h>

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static remote_leg_input_t s_input;
static uint8_t s_contact_mask;
static bool s_fall_detected;
static ble_input_rx_log_fn s_rx_log;
static ble_input_invalid_log_fn s_invalid_log;
static ble_input_disconnect_fn s_disconnect_hook;
static ble_input_audio_play_fn s_audio_play_hook;
static bool s_link_secure;
static bool s_home_requested;
static bool s_home_processing;
static bool s_home_complete;
static uint16_t s_telemetry_sequence;
static uint8_t s_telemetry_flags;
static uint16_t s_accel_centi_g;
static uint16_t s_telemetry_age_ms;
static bool s_status_pending;
static bool s_telemetry_pending;
static uint8_t s_game_mode;

#define QW_BLE_TX_PERIOD_MS 100U

static void ble_tx_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(QW_BLE_TX_PERIOD_MS));
        bool status_pending;
        bool telemetry_pending;
        portENTER_CRITICAL(&s_lock);
        status_pending = s_status_pending;
        telemetry_pending = s_telemetry_pending;
        s_status_pending = false;
        s_telemetry_pending = false;
        portEXIT_CRITICAL(&s_lock);
        if (status_pending) {
            uint8_t packet[QW_BLE_PACKET_LEN];
            ble_input_build_status_packet(packet);
            ble_gatt_notify_status(packet);
        }
        if (telemetry_pending) {
            uint8_t packet[QW_BLE_TELEMETRY_PACKET_LEN];
            ble_input_build_telemetry_packet(packet);
            ble_gatt_notify_telemetry(packet);
        }
    }
}

static int64_t watchdog_timeout_us(void)
{
    return (int64_t)CONFIG_NIULAI_WATCHDOG_TIMEOUT_MS * 1000;
}

void ble_input_init(void)
{
    qw_remote_input_init(&s_input);
    s_contact_mask = 0;
    s_fall_detected = false;
    s_link_secure = false;
    s_home_requested = false;
    s_home_processing = false;
    s_home_complete = false;
    s_telemetry_sequence = 0U;
    s_telemetry_flags = 0U;
    s_accel_centi_g = 0U;
    s_telemetry_age_ms = UINT16_MAX;
    s_status_pending = false;
    s_telemetry_pending = false;
    s_game_mode = QW_BLE_GAME_MODE_SOLO;
    s_audio_play_hook = NULL;
    ble_gatt_init();
    (void)xTaskCreate(ble_tx_task, "ble_tx", 4096U, NULL, 5U, NULL);
}

uint8_t ble_input_game_mode(void)
{
    uint8_t mode;
    portENTER_CRITICAL(&s_lock);
    mode = s_game_mode;
    portEXIT_CRITICAL(&s_lock);
    return mode;
}

bool ble_input_set_game_mode(uint8_t mode)
{
    if (mode != QW_BLE_GAME_MODE_SOLO && mode != QW_BLE_GAME_MODE_MULTI) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    s_game_mode = mode;
    portEXIT_CRITICAL(&s_lock);
    return true;
}

bool ble_input_request_mama_audio(void)
{
    const ble_input_audio_play_fn audio_play_hook = s_audio_play_hook;
    return audio_play_hook != NULL && audio_play_hook();
}

void ble_input_set_log_hooks(ble_input_rx_log_fn rx_log,
                             ble_input_invalid_log_fn invalid_log)
{
    s_rx_log = rx_log;
    s_invalid_log = invalid_log;
}

void ble_input_set_disconnect_hook(ble_input_disconnect_fn disconnect_hook)
{
    s_disconnect_hook = disconnect_hook;
}

void ble_input_set_audio_play_hook(ble_input_audio_play_fn audio_play_hook)
{
    s_audio_play_hook = audio_play_hook;
}

bool ble_input_link_secure(void)
{
    bool secure;
    portENTER_CRITICAL(&s_lock);
    secure = s_link_secure;
    portEXIT_CRITICAL(&s_lock);
    return secure;
}

void ble_input_set_link_secure(bool secure)
{
    bool became_unsecure;
    portENTER_CRITICAL(&s_lock);
    became_unsecure = s_link_secure && !secure;
    s_link_secure = secure;
    /* Security state changes start a fresh input session. This prevents a
     * packet accepted before pairing from satisfying the post-pairing gate. */
    qw_remote_input_clear(&s_input);
    s_home_requested = false;
    s_home_processing = false;
    s_home_complete = false;
    portEXIT_CRITICAL(&s_lock);
    ble_input_notify_status();
    if (became_unsecure && s_disconnect_hook != NULL) {
        s_disconnect_hook();
    }
}

remote_leg_input_t ble_input_snapshot(void)
{
    remote_leg_input_t snap;
    portENTER_CRITICAL(&s_lock);
    snap = s_input;
    portEXIT_CRITICAL(&s_lock);
    return snap;
}

ble_input_gate_snapshot_t ble_input_gate_snapshot(void)
{
    ble_input_gate_snapshot_t snap;
    portENTER_CRITICAL(&s_lock);
    snap.input = s_input;
    snap.link_secure = s_link_secure;
    portEXIT_CRITICAL(&s_lock);
    return snap;
}

bool ble_input_is_pressed(uint8_t leg)
{
    remote_leg_input_t snap = ble_input_snapshot();
    return leg < 4 && (snap.leg_mask & (1u << leg)) != 0;
}

uint8_t ble_input_contact_mask(void)
{
    uint8_t contact_mask;
    portENTER_CRITICAL(&s_lock);
    contact_mask = s_contact_mask;
    portEXIT_CRITICAL(&s_lock);
    return contact_mask;
}

void ble_input_set_contact_mask(uint8_t contact_mask)
{
    bool changed;
    contact_mask &= (uint8_t)~QW_BLE_MASK_RESERVED;
    portENTER_CRITICAL(&s_lock);
    changed = s_contact_mask != contact_mask;
    s_contact_mask = contact_mask;
    portEXIT_CRITICAL(&s_lock);

    if (changed) {
        ble_input_notify_status();
    }
}

void ble_input_set_fall_detected(bool detected)
{
    bool changed;
    portENTER_CRITICAL(&s_lock);
    changed = s_fall_detected != detected;
    if (detected && !s_fall_detected) {
        s_input.leg_mask = 0U;
    }
    s_fall_detected = detected;
    portEXIT_CRITICAL(&s_lock);
    if (changed) {
        ble_input_notify_status();
    }
}

void ble_input_build_status_packet(uint8_t out[QW_BLE_PACKET_LEN])
{
    remote_leg_input_t snap;
    uint8_t contact_mask;
    uint8_t flags;
    bool fall_detected;
    bool home_complete;

    portENTER_CRITICAL(&s_lock);
    snap = s_input;
    contact_mask = s_contact_mask;
    fall_detected = s_fall_detected;
    home_complete = s_home_complete;
    portEXIT_CRITICAL(&s_lock);

    flags = snap.failsafe_active ? QW_BLE_STATUS_FLAG_FAILSAFE_ACTIVE
                                 : QW_BLE_FLAGS_NONE;
    flags |= (uint8_t)(contact_mask << QW_BLE_STATUS_CONTACT_MASK_SHIFT);
    if (fall_detected) flags |= QW_BLE_STATUS_FLAG_FALL_DETECTED;
    if (home_complete) flags |= QW_BLE_STATUS_FLAG_HOME_COMPLETE;
    qw_ble_build_status_packet(
        out, snap.sequence, snap.leg_mask, flags & QW_BLE_STATUS_FLAGS_MASK);
}

void ble_input_notify_status(void)
{
    portENTER_CRITICAL(&s_lock);
    s_status_pending = true;
    portEXIT_CRITICAL(&s_lock);
}

void ble_input_set_accel_telemetry(bool valid, bool clipped,
                                   float dynamic_accel_g, uint32_t age_ms)
{
    uint8_t flags = clipped ? QW_BLE_TELEMETRY_FLAG_CLIPPED : 0U;
    uint16_t centi_g = 0U;
    if (valid && isfinite(dynamic_accel_g) && dynamic_accel_g >= 0.0f) {
        float scaled = dynamic_accel_g * 100.0f;
        if (scaled > (float)UINT16_MAX) scaled = (float)UINT16_MAX;
        centi_g = (uint16_t)(scaled + 0.5f);
        flags |= QW_BLE_TELEMETRY_FLAG_VALID;
    }
    if (age_ms > UINT16_MAX) age_ms = UINT16_MAX;
    if (age_ms > 1000U) flags |= QW_BLE_TELEMETRY_FLAG_STALE;

    portENTER_CRITICAL(&s_lock);
    s_telemetry_flags = flags;
    s_accel_centi_g = centi_g;
    s_telemetry_age_ms = (uint16_t)age_ms;
    s_telemetry_sequence++;
    s_telemetry_pending = true;
    portEXIT_CRITICAL(&s_lock);
}

void ble_input_build_telemetry_packet(
    uint8_t out[QW_BLE_TELEMETRY_PACKET_LEN])
{
    uint8_t flags;
    uint16_t accel_centi_g;
    uint16_t age_ms;
    uint16_t sequence;
    portENTER_CRITICAL(&s_lock);
    flags = s_telemetry_flags;
    accel_centi_g = s_accel_centi_g;
    age_ms = s_telemetry_age_ms;
    sequence = s_telemetry_sequence;
    portEXIT_CRITICAL(&s_lock);
    qw_ble_build_telemetry_packet(out, flags, accel_centi_g, age_ms, sequence);
}

void ble_input_notify_telemetry(void)
{
    portENTER_CRITICAL(&s_lock);
    s_telemetry_pending = true;
    portEXIT_CRITICAL(&s_lock);
}

bool ble_input_request_home(void)
{
    bool accepted = false;
    bool status_changed = false;
    portENTER_CRITICAL(&s_lock);
    /* Latch after authentication even if the preceding neutral CONTROL_STATE
     * write has not become visible yet. The control task still requires a
     * neutral mask before it consumes the request. */
    if (qw_ble_home_latch_allowed(
            s_link_secure, s_home_requested || s_home_processing)) {
        s_home_requested = true;
        status_changed = s_home_complete;
        s_home_complete = false;
        accepted = true;
    }
    portEXIT_CRITICAL(&s_lock);
    if (status_changed) {
        ble_input_notify_status();
    }
    return accepted;
}

bool ble_input_take_home_request(void)
{
    bool requested = false;
    portENTER_CRITICAL(&s_lock);
    if (s_home_requested &&
        qw_ble_home_request_allowed(s_link_secure, s_input.leg_mask == 0U,
                                    false)) {
        s_home_requested = false;
        s_home_processing = true;
        requested = true;
    }
    portEXIT_CRITICAL(&s_lock);
    return requested;
}

void ble_input_complete_home(void)
{
    portENTER_CRITICAL(&s_lock);
    s_home_processing = false;
    s_home_complete = true;
    portEXIT_CRITICAL(&s_lock);
    ble_input_notify_status();
}

bool ble_input_watchdog_check(int64_t now_us)
{
    bool changed;
    portENTER_CRITICAL(&s_lock);
    changed = qw_remote_input_apply_failsafe(&s_input, now_us,
                                              watchdog_timeout_us());
    portEXIT_CRITICAL(&s_lock);
    return changed;
}

void ble_input_handle_disconnect(void)
{
    portENTER_CRITICAL(&s_lock);
    s_link_secure = false;
    qw_remote_input_clear(&s_input);
    s_home_requested = false;
    s_home_processing = false;
    s_home_complete = false;
    portEXIT_CRITICAL(&s_lock);

    if (s_disconnect_hook != NULL) {
        s_disconnect_hook();
    }
}

bool ble_input_accept_packet(uint8_t seq, uint8_t mask)
{
    int64_t now_us = esp_timer_get_time();
    bool mask_changed;
    bool failsafe_changed;
    bool home_status_changed = false;
    portENTER_CRITICAL(&s_lock);
    if (!qw_ble_control_request_allowed(
            s_link_secure, s_home_requested || s_home_processing,
            mask == 0U)) {
        portEXIT_CRITICAL(&s_lock);
        ble_input_log_invalid();
        return false;
    }
    const uint8_t previous_mask = s_input.leg_mask;
    const bool previous_failsafe = s_input.failsafe_active;
    qw_remote_input_accept(&s_input, seq, s_fall_detected ? 0U : mask, now_us);
    if (mask != 0U && s_home_complete) {
        s_home_complete = false;
        home_status_changed = true;
    }
    mask_changed = previous_mask != s_input.leg_mask;
    failsafe_changed = previous_failsafe != s_input.failsafe_active;
    portEXIT_CRITICAL(&s_lock);

    if (s_rx_log != NULL) {
        s_rx_log(seq, mask);
    }
    if (mask_changed || failsafe_changed || home_status_changed) {
        ble_input_notify_status();
    }
    return true;
}

void ble_input_log_invalid(void)
{
    if (s_invalid_log != NULL) {
        s_invalid_log();
    }
}
