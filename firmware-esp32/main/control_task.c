#include "sdkconfig.h"

#include "control_task.h"
#include "control_admission.h"

#include <math.h>
#include <string.h>

#include "ble_input.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "foot_contact.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "motion_strategy.h"
#include "qw_hal_espidf.h"
#include "runtime_imu.h"
#include "safety_gate.h"
#include "uart_log.h"

#ifndef CONFIG_NIULAI_ARD1_LIFT_DURATION_MS
#define CONFIG_NIULAI_ARD1_LIFT_DURATION_MS 300
#endif
#ifndef CONFIG_NIULAI_ARD1_SWING_DURATION_MS
#define CONFIG_NIULAI_ARD1_SWING_DURATION_MS 360
#endif
#ifndef CONFIG_NIULAI_ARD1_SEARCH_DURATION_MS
#define CONFIG_NIULAI_ARD1_SEARCH_DURATION_MS 120
#endif
#ifndef CONFIG_NIULAI_ARD1_STEP_LENGTH_MM
#define CONFIG_NIULAI_ARD1_STEP_LENGTH_MM 30
#endif
#ifndef CONFIG_NIULAI_ARD1_LIFT_Z_MM
#define CONFIG_NIULAI_ARD1_LIFT_Z_MM -60
#endif
#ifndef CONFIG_NIULAI_ARD1_TOUCHDOWN_Z_MM
#define CONFIG_NIULAI_ARD1_TOUCHDOWN_Z_MM -100
#endif

/* The control loop updates the latest telemetry snapshot. BLE TX is owned by
 * the separate ble_tx task in the ble_input component. */
#define QW_DIAGNOSTIC_STACK_LOG_PERIOD 25U

static const char *TAG = "control_diag";

static void diagnostic_stack_log(const char *where)
{
    UBaseType_t free_stack = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "stack where=%s free=%u", where, (unsigned)free_stack);
}

static qw_motion_strategy_t s_motion;
static StaticSemaphore_t s_fsm_mutex_storage;
static SemaphoreHandle_t s_fsm_mutex;
static bool s_runner_initialized;
static qw_motion_frame_t s_last_frame;
static uint64_t s_last_transition_total;
static uint8_t s_last_contact_mask;
static qw_hal_clock_t s_clock;
static bool s_have_logged_command;
static uint8_t s_last_logged_command_mask;
static leg_command_mode_t s_last_logged_command_modes[QW_MOTION_MAX_LEGS];
static qw_control_admission_t s_admission;

static bool read_pressed_cb(void *ctx, uint8_t leg)
{
    (void)ctx;
    return leg < QW_MOTION_MAX_LEGS && ble_input_is_pressed(leg);
}

static bool read_contact_cb(void *ctx, uint8_t leg)
{
    (void)ctx;
    return leg < QW_MOTION_MAX_LEGS &&
           (foot_contact_mask() & (uint8_t)(1U << leg)) != 0U;
}

static void emit_frame_cb(const qw_motion_frame_t *frame, void *ctx)
{
    (void)ctx;
    if (frame == NULL) {
        return;
    }
    s_last_frame = *frame;
    bool command_changed = !s_have_logged_command ||
                           s_last_logged_command_mask != frame->command_mask;
    for (uint8_t i = 0; i < QW_MOTION_MAX_LEGS; i++) {
        if ((frame->command_mask & (uint8_t)(1U << i)) != 0U) {
            const leg_command_t *command = &frame->legs[i];
            if (!s_have_logged_command ||
                s_last_logged_command_modes[i] != command->mode) {
                command_changed = true;
            }
        }
    }
    if (command_changed) {
        for (uint8_t i = 0; i < QW_MOTION_MAX_LEGS; i++) {
            if ((frame->command_mask & (uint8_t)(1U << i)) != 0U) {
                const leg_command_t *command = &frame->legs[i];
                qw_uart_log_fsm_cmd(leg_command_mode_name(command->mode),
                                    command->progress);
                break;
            }
        }
        s_last_logged_command_mask = frame->command_mask;
        for (uint8_t i = 0; i < QW_MOTION_MAX_LEGS; i++) {
            s_last_logged_command_modes[i] = frame->legs[i].mode;
        }
        s_have_logged_command = true;
    }
}

void control_task_sync_init(void)
{
    s_fsm_mutex = xSemaphoreCreateMutexStatic(&s_fsm_mutex_storage);
    s_runner_initialized = false;
    s_last_contact_mask = 0;
    memset(&s_last_frame, 0, sizeof(s_last_frame));
    s_have_logged_command = false;
    s_last_logged_command_mask = 0U;
    memset(s_last_logged_command_modes, 0, sizeof(s_last_logged_command_modes));
    s_admission = (qw_control_admission_t){0};
}

void control_task(void *arg)
{
    (void)arg;
    s_clock = qw_hal_espidf_clock();
    const qw_ard1_config_t cfg = {
        .lift_duration_ms = CONFIG_NIULAI_ARD1_LIFT_DURATION_MS,
        .swing_duration_ms = CONFIG_NIULAI_ARD1_SWING_DURATION_MS,
        .search_duration_ms = CONFIG_NIULAI_ARD1_SEARCH_DURATION_MS,
        .step_length_mm = CONFIG_NIULAI_ARD1_STEP_LENGTH_MM,
        .lift_z_mm = CONFIG_NIULAI_ARD1_LIFT_Z_MM,
        .touchdown_z_mm = CONFIG_NIULAI_ARD1_TOUCHDOWN_Z_MM,
    };
    const qw_motion_init_args_t init_args = {
        .strategy_config = &cfg,
        .leg_mask = 0x0FU,
        .read_pressed = read_pressed_cb,
        .read_contact = read_contact_cb,
        .emit = emit_frame_cb,
        .capability_ctx = NULL,
    };

    xSemaphoreTake(s_fsm_mutex, portMAX_DELAY);
    s_runner_initialized = qw_motion_ard1_create(&s_motion) &&
                           qw_motion_strategy_init(&s_motion, &init_args);
    if (s_runner_initialized) {
        qw_motion_snapshot_t snapshot;
        s_runner_initialized = qw_motion_strategy_snapshot(&s_motion, &snapshot);
        s_last_transition_total =
            s_runner_initialized ? snapshot.transition_total : 0U;
    }
    xSemaphoreGive(s_fsm_mutex);
    if (!s_runner_initialized) {
        /* Config is validated at build time by Kconfig ranges, but never
         * arm anything on the off chance: /OE stays high. */
        qw_uart_log_control_init_failed();
        vTaskDelete(NULL);
    }
    safety_gate_notify_control_ready();

    TickType_t wake = xTaskGetTickCount();
    unsigned diagnostic_cycle = 0U;
    bool home_pending = false;
    int64_t last_us = s_clock.now_us(s_clock.ctx);
    for (;;) {
        const bool diagnostic_log =
            (diagnostic_cycle++ % QW_DIAGNOSTIC_STACK_LOG_PERIOD) == 0U;
        if (diagnostic_log) {
            diagnostic_stack_log("loop_begin");
        }
        int64_t now_us = s_clock.now_us(s_clock.ctx);
        float dt_s = (float)(now_us - last_us) / 1e6f;
        last_us = now_us;
        const float nominal_dt_s =
            1.0f / (float)CONFIG_NIULAI_CONTROL_TICK_HZ;
        if (!isfinite(dt_s) || dt_s <= 0.0f || dt_s > 2.0f * nominal_dt_s) {
            dt_s = nominal_dt_s;
        }

        foot_contact_sample();
        uint8_t contact_mask = foot_contact_mask();
        if (contact_mask != s_last_contact_mask) {
            s_last_contact_mask = contact_mask;
            qw_uart_log_foot_contact(contact_mask);
            ble_input_set_contact_mask(contact_mask);
        }

        qw_runtime_imu_snapshot_t imu;
        if (qw_runtime_imu_snapshot(&imu)) {
            ble_input_set_accel_telemetry(true, imu.clipped,
                                          imu.accel_magnitude_g, imu.age_ms);
        } else {
            ble_input_set_accel_telemetry(false, false, 0.0f, UINT32_MAX);
        }
        const bool fall_detected = qw_runtime_imu_fall_detected();
        ble_input_set_fall_detected(fall_detected);
        safety_gate_notify_fall(fall_detected);
        if (ble_input_take_home_request()) {
            if (ble_input_link_secure()) {
                control_fsm_reset();
                home_pending = true;
                qw_uart_log_ble_home();
            } else {
                qw_uart_log_ble_home_rejected();
            }
        }

        const remote_leg_input_t input = ble_input_snapshot();
        const qw_control_admission_result_t admission =
            qw_control_admission_step(&s_admission, safety_gate_armed(),
                                      input.leg_mask);
        if (admission.reset_fsm) {
            /* Discard every phase/edge observed while the actuator was
             * disarmed. The next non-neutral packet must be a new press. */
            control_fsm_reset();
        }

        xSemaphoreTake(s_fsm_mutex, portMAX_DELAY);
        if (admission.allow_tick) {
            qw_motion_strategy_tick(&s_motion, dt_s);
        }
        if (diagnostic_log) {
            diagnostic_stack_log(admission.allow_tick ? "after_motion" :
                                 "motion_not_admitted");
        }

        qw_motion_snapshot_t snapshot;
        bool have_snapshot =
            qw_motion_strategy_snapshot(&s_motion, &snapshot);
        if (have_snapshot &&
            snapshot.transition_total != s_last_transition_total) {
            if (snapshot.transition_count != 0U) {
                const qw_motion_transition_t *transition =
                    &snapshot.transitions[snapshot.transition_count - 1U];
                qw_uart_log_fsm_evt(transition->from_state,
                                    transition->to_state, transition->reason,
                                    (unsigned)(transition->elapsed_s * 1000.0f));
            }
            s_last_transition_total = snapshot.transition_total;
        }

        qw_motion_state_code_t state =
            have_snapshot ? snapshot.state : QW_MOTION_STATE_FAULT;
        qw_motion_frame_t frame = s_last_frame;
        xSemaphoreGive(s_fsm_mutex);

        safety_gate_notify_motion_state(state);
        bool applied = false;
        if (admission.allow_tick || safety_gate_armed()) {
            /* While waiting for the post-arm neutral packet, this is only the
             * calibrated hold frame. It is never committed as a motion tick. */
            applied = safety_gate_apply(&frame);
        }
        if (diagnostic_log) {
            diagnostic_stack_log(applied ? "after_actuator_applied" :
                                 "after_actuator_not_applied");
        }
        if (home_pending && applied && frame.command_mask == 0x0FU &&
            frame.joint_target_mask == 0U) {
            bool all_hold = true;
            for (uint8_t leg = 0U; leg < QW_MOTION_MAX_LEGS; leg++) {
                if (frame.legs[leg].mode != LEG_CMD_HOLD_SUPPORT) {
                    all_hold = false;
                    break;
                }
            }
            if (all_hold) {
                ble_input_complete_home();
                home_pending = false;
            }
        }
        if (admission.allow_tick) {
            xSemaphoreTake(s_fsm_mutex, portMAX_DELAY);
            qw_motion_strategy_commit(&s_motion, applied);
            xSemaphoreGive(s_fsm_mutex);
        }

        vTaskDelayUntil(&wake,
                        pdMS_TO_TICKS(1000 / CONFIG_NIULAI_CONTROL_TICK_HZ));
    }
}

bool control_fsm_snapshot(qw_motion_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return false;
    }
    xSemaphoreTake(s_fsm_mutex, portMAX_DELAY);
    bool initialized = s_runner_initialized &&
                       qw_motion_strategy_snapshot(&s_motion, snapshot);
    xSemaphoreGive(s_fsm_mutex);
    return initialized;
}

void control_fsm_reset(void)
{
    xSemaphoreTake(s_fsm_mutex, portMAX_DELAY);
    if (s_runner_initialized) {
        qw_motion_strategy_reset(&s_motion);
        s_last_frame = (qw_motion_frame_t){0};
        s_last_frame.command_mask = 0x0FU;
        for (uint8_t leg = 0; leg < QW_MOTION_MAX_LEGS; leg++) {
            s_last_frame.legs[leg] =
                (leg_command_t){LEG_CMD_HOLD_SUPPORT, 0.0f};
        }
        qw_motion_snapshot_t snapshot;
        if (qw_motion_strategy_snapshot(&s_motion, &snapshot)) {
            s_last_transition_total = snapshot.transition_total;
        }
        qw_uart_log_fsm_reset();
    }
    xSemaphoreGive(s_fsm_mutex);
    safety_gate_notify_motion_state(QW_MOTION_STATE_SUPPORT);
    safety_gate_clear_stop_latch();
}
