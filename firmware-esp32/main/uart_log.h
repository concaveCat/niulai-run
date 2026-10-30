/* Single owner of every UART log-line format for the Niulai endpoint.
 *
 * BLE_CTRL_RX is verbatim from docs/BLE_PROTOCOL.md; the other lines follow
 * the plan (not pinned by the protocol document). Components cannot include
 * this header (main/ sits downstream of components), so the BLE component
 * emits BLE_CTRL_RX / BLE_CTRL_RX_INVALID through the log hooks registered
 * in app_main (ble_input_set_log_hooks) and ble_gatt.c owns its own
 * BLE_GATT_REGISTER boot lines.
 */
#ifndef UART_LOG_H
#define UART_LOG_H

#include "sdkconfig.h"

#include <stdint.h>
#include <stdio.h>

/* BLE_CTRL_RX seq=52 mask=0x09 FL=1 FR=0 RL=0 RR=1 */
static inline void qw_uart_log_ble_ctrl_rx(uint8_t seq, uint8_t mask)
{
    printf("BLE_CTRL_RX seq=%u mask=0x%02X FL=%u FR=%u RL=%u RR=%u\n",
           (unsigned)seq, (unsigned)mask,
           (unsigned)((mask >> 0) & 1u), (unsigned)((mask >> 1) & 1u),
           (unsigned)((mask >> 2) & 1u), (unsigned)((mask >> 3) & 1u));
}

static inline void qw_uart_log_ble_ctrl_rx_invalid(void)
{
    printf("BLE_CTRL_RX_INVALID\n");
}

/* Debounced KW10 state; a set bit is the current physical contact state. */
static inline void qw_uart_log_foot_contact(uint8_t mask)
{
    printf("FOOT_CONTACT mask=0x%02X FL=%u FR=%u RL=%u RR=%u\n",
           (unsigned)mask, (unsigned)((mask >> 0) & 1u),
           (unsigned)((mask >> 1) & 1u), (unsigned)((mask >> 2) & 1u),
           (unsigned)((mask >> 3) & 1u));
}

static inline void qw_uart_log_boot(const char *stage, const char *leg)
{
    printf("牛来快跑 firmware-esp32 stage=%s active_leg=%s\n", stage, leg);
}

static inline void qw_uart_log_fsm_evt(const char *from, const char *to,
                                       const char *reason,
                                       unsigned elapsed_ms)
{
    printf("FSM_EVT from=%s to=%s reason=%s elapsed_ms=%u\n", from, to,
           reason, elapsed_ms);
}

static inline void qw_uart_log_fsm_cmd(const char *mode, float progress)
{
    printf("FSM_CMD mode=%s progress=%.3f\n", mode, (double)progress);
}

static inline void qw_uart_log_fsm_reset(void)
{
    printf("FSM_RESET state=SUPPORT\n");
}

static inline void qw_uart_log_ble_home(void)
{
    printf("BLE_HOME_REQUEST accepted=1\n");
}

static inline void qw_uart_log_ble_home_rejected(void)
{
    printf("BLE_HOME_REQUEST accepted=0 reason=non_neutral_or_unsecure\n");
}

static inline void qw_uart_log_control_init_failed(void)
{
    printf("CONTROL_TASK init_failed\n");
}

static inline void qw_uart_log_task_create_failed(const char *task)
{
    printf("APP_MAIN %s_task_create_failed\n", task);
}

static inline void qw_uart_log_uart_prompt(void)
{
    printf("UART commands: reset | status | transitions\n");
}

static inline void qw_uart_log_unknown_command(const char *line)
{
    printf("APP_MAIN unknown_command: %s", line);
}

static inline void qw_uart_log_watchdog(int64_t last_rx_us)
{
    printf("WATCHDOG failsafe=1 mask=0 timeout_ms=%u last_rx_us=%lld\n",
           (unsigned)CONFIG_NIULAI_WATCHDOG_TIMEOUT_MS,
           (long long)last_rx_us);
}

static inline void qw_uart_log_safety_gate_opened(void)
{
    printf("SAFETY_GATE opened\n");
}

static inline void qw_uart_log_safety_gate_blocked(const char *reason)
{
    printf("SAFETY_GATE blocked reason=%s\n", reason);
}

static inline void qw_uart_log_safety_gate_init_failed(void)
{
    printf("SAFETY_GATE pca9685_init_failed staying_on_null_actuator\n");
}

static inline void qw_uart_log_status(const char *stage, const char *fsm,
                                      unsigned acceptance_ticks,
                                      unsigned required_ticks, int armed,
                                      int failsafe)
{
    printf("QW_STATUS stage=%s fsm=%s acceptance=%u/%u armed=%d failsafe=%d\n",
           stage, fsm, acceptance_ticks, required_ticks, armed, failsafe);
}

static inline void qw_uart_log_transition(unsigned index, const char *from,
                                          const char *to, const char *reason,
                                          unsigned elapsed_ms)
{
    printf("TRANSITION[%u] %s -> %s reason=%s elapsed_ms=%u\n", index, from,
           to, reason, elapsed_ms);
}

static inline void qw_uart_log_transitions_header(unsigned long long total)
{
    printf("TRANSITIONS total=%llu\n", total);
}

#endif /* UART_LOG_H */
