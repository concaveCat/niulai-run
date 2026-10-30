/* BLE remote input adapter: shared state, watchdog, GATT server.
 * The mask is a replaceable input capability only - it must never be wired
 * directly to actuators (docs/BLE_PROTOCOL.md section 8). */
#ifndef BLE_INPUT_H
#define BLE_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "ble_protocol.h"
#include "remote_input_state.h"

/* Log hooks: uart_log.h in main/ owns all log-line formats, so this
 * component reports through function pointers registered by app_main. */
typedef void (*ble_input_rx_log_fn)(uint8_t seq, uint8_t mask);
typedef void (*ble_input_invalid_log_fn)(void);
typedef void (*ble_input_disconnect_fn)(void);
typedef bool (*ble_input_audio_play_fn)(void);

/* Initializes the shared state, the NimBLE host and the GATT server. */
void ble_input_init(void);

void ble_input_set_log_hooks(ble_input_rx_log_fn rx_log,
                             ble_input_invalid_log_fn invalid_log);
void ble_input_set_disconnect_hook(ble_input_disconnect_fn disconnect_hook);
void ble_input_set_audio_play_hook(ble_input_audio_play_fn audio_play_hook);

typedef struct {
    remote_leg_input_t input;
    bool link_secure;
} ble_input_gate_snapshot_t;

/* Control writes and physical actuation require an encrypted bonded BLE link.
 * The NimBLE GAP layer updates this state after encryption changes. */
bool ble_input_link_secure(void);
void ble_input_set_link_secure(bool secure);

/* Consistent copy of the shared state (spinlock-guarded). */
remote_leg_input_t ble_input_snapshot(void);

/* Secure-link input snapshot used by the safety gate. Both fields come
 * from one critical section so latch-reset decisions cannot mix sessions. */
ble_input_gate_snapshot_t ble_input_gate_snapshot(void);

/* Debounced-free level read: leg 0..3 = FL/FR/RL/RR bit of the mask. */
bool ble_input_is_pressed(uint8_t leg);

/* Notifies STATUS to the connected peer (no-op without a connection). */
void ble_input_notify_status(void);

/* Returns/updates the debounced physical contact mask. A changed mask causes
 * a STATUS notification; this is separate from the remote input mask. */
uint8_t ble_input_contact_mask(void);
void ble_input_set_contact_mask(uint8_t contact_mask);

/* Updates the read-only MPU6500 fall state. A rising state clears the remote
 * leg mask so the next actuator frame cannot reuse a held input. */
void ble_input_set_fall_detected(bool detected);

/* Builds the current STATUS packet for both GATT reads and notifications. */
void ble_input_build_status_packet(uint8_t out[QW_BLE_PACKET_LEN]);

/* Updates the read-only MPU6500 dynamic-acceleration snapshot. */
void ble_input_set_accel_telemetry(bool valid, bool clipped,
                                   float dynamic_accel_g, uint32_t age_ms);
void ble_input_build_telemetry_packet(
    uint8_t out[QW_BLE_TELEMETRY_PACKET_LEN]);
void ble_input_notify_telemetry(void);

/* Display-only game mode; it never participates in control admission. */
uint8_t ble_input_game_mode(void);
bool ble_input_set_game_mode(uint8_t mode);

/* Authenticated non-motion audio action. The hook must only enqueue playback
 * and return; it must not block the NimBLE host. */
bool ble_input_request_mama_audio(void);

/* HOME_COMMAND is latched here and consumed by the control task. The
 * completion bit is raised only after the actuator accepts the calibrated
 * all-legs formal-rest frame. */
bool ble_input_request_home(void);
bool ble_input_take_home_request(void);
void ble_input_complete_home(void);

/* Watchdog check against esp_timer time. On expiry (now - last_rx >=
 * timeout) and not already failed: zeroes the mask, sets FAILSAFE_ACTIVE
 * and returns true (state changed). */
bool ble_input_watchdog_check(int64_t now_us);

/* Clears remote input synchronously when the BLE link is lost. */
void ble_input_handle_disconnect(void);

/* Applies one valid control packet: updates seq/mask/last_rx_time_us,
 * clears FAILSAFE_ACTIVE, logs BLE_CTRL_RX and notifies STATUS. Called from
 * the NimBLE GATT write callback (keep short: spinlock only, no delay). */
bool ble_input_accept_packet(uint8_t seq, uint8_t mask);

/* Reports a rejected control packet (BLE_CTRL_RX_INVALID hook). */
void ble_input_log_invalid(void);

/* NimBLE GATT server internals (ble_gatt.c). */
void ble_gatt_init(void);
void ble_gatt_notify_status(const uint8_t pkt[QW_BLE_PACKET_LEN]);
void ble_gatt_notify_telemetry(
    const uint8_t pkt[QW_BLE_TELEMETRY_PACKET_LEN]);

#endif /* BLE_INPUT_H */
