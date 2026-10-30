#include "safety_gate.h"

#include "sdkconfig.h"

#include <math.h>
#include <stdint.h>

#include "ble_input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "leg_actuator.h"
#include "safety_gate_contract.h"
#include "safety_policy.h"
#include "uart_log.h"

/* The watchdog task feeds one acceptance sample per 100 ms tick. */
#define QW_GATE_TICK_PERIOD_MS 100
#define QW_GATE_IO_LOCK_TIMEOUT_MS 100

typedef enum {
    QW_GATE_ACTION_NONE = 0,
    QW_GATE_ACTION_OPEN,
    QW_GATE_ACTION_CLOSE,
} qw_gate_action_t;

static qw_stage_t s_stage;
static StaticSemaphore_t s_mutex_storage;
static StaticSemaphore_t s_io_mutex_storage;
static StaticSemaphore_t s_oe_mutex_storage;
static SemaphoreHandle_t s_mutex;
static SemaphoreHandle_t s_io_mutex;
static SemaphoreHandle_t s_oe_mutex;
static const leg_actuator_t *s_actuator;
static bool s_armed;
static bool s_actuator_ready;
static bool s_control_ready;
static bool s_contact_ready;
static bool s_failsafe;
static bool s_fall_detected;
static bool s_motion_fault;
static bool s_actuator_fault;
static bool s_transition_in_progress;
static bool s_open_in_progress;
static bool s_abort_open;
static bool s_stop_latched;
static bool s_safe_stop_pending;
static uint32_t s_authorization_epoch;
static int s_acceptance_ticks;

static int required_acceptance_ticks(void)
{
    return CONFIG_NIULAI_ACTUATION_ACCEPTANCE_WINDOW_S * 1000 /
           QW_GATE_TICK_PERIOD_MS;
}

static bool acceptance_ok_locked(void)
{
    return s_acceptance_ticks >= required_acceptance_ticks();
}

static bool gate_should_be_open_locked(void)
{
    if (s_fall_detected) return false;
    return qw_safety_policy_should_arm((qw_safety_policy_input_t){
        .actuation_enabled = s_stage == QW_STAGE_ACTUATION_ENABLED,
        .acceptance_ok = acceptance_ok_locked(),
        .actuator_ready = s_actuator_ready,
        .control_ready = s_control_ready,
        .contact_ready = s_contact_ready,
        .ble_link_secure = ble_input_link_secure(),
        .stop_latched = s_stop_latched,
        .failsafe = s_failsafe,
        .motion_fault = s_motion_fault,
        .actuator_fault = s_actuator_fault,
    });
}

static const char *block_reason_locked(void)
{
    if (s_fall_detected) {
        return "imu_fall";
    }
    if (s_failsafe) {
        return "failsafe";
    }
    if (s_motion_fault) {
        return "motion_fault";
    }
    if (s_stop_latched) {
        return "stop_latched";
    }
    if (s_actuator_fault) {
        return "actuator_fault";
    }
    if (!s_contact_ready) {
        return "contact_unavailable";
    }
    if (!s_actuator_ready) {
        return "actuator_unavailable";
    }
    if (!s_control_ready) {
        return "control_unavailable";
    }
    if (!acceptance_ok_locked()) {
        return "acceptance_break";
    }
    return "stage_disabled";
}

static qw_gate_action_t plan_action_locked(void)
{
    const bool authorized = gate_should_be_open_locked();
    if (s_safe_stop_pending) {
        s_transition_in_progress = true;
        s_open_in_progress = false;
        return QW_GATE_ACTION_CLOSE;
    }
    if (s_transition_in_progress) {
        if (!authorized && s_open_in_progress) {
            s_abort_open = true;
        }
        return QW_GATE_ACTION_NONE;
    }
    if (authorized && !s_armed) {
        s_transition_in_progress = true;
        s_open_in_progress = true;
        s_abort_open = false;
        return QW_GATE_ACTION_OPEN;
    }
    if (!authorized && s_armed) {
        s_transition_in_progress = true;
        s_open_in_progress = false;
        return QW_GATE_ACTION_CLOSE;
    }
    return QW_GATE_ACTION_NONE;
}

static bool take_io_lock(void)
{
    return xSemaphoreTake(s_io_mutex,
                          pdMS_TO_TICKS(QW_GATE_IO_LOCK_TIMEOUT_MS)) == pdTRUE;
}

/* Every /OE write is serialized. The final authorization check is performed
 * while this short GPIO critical section is reserved, so a concurrent revoke
 * can only win before the enable write or after it with a final disable. */
static bool set_oe_serialized(bool level)
{
    if (xSemaphoreTake(s_oe_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool ok = qw_actuator_pca9685.set_oe(level);
    xSemaphoreGive(s_oe_mutex);
    return ok;
}

/* State that can revoke an armed actuator is normally changed only while the
 * I/O lock is held. If a bounded wait expires, /OE is asserted first and the
 * authorization epoch is changed anyway; an in-flight writer will observe the
 * epoch mismatch and fail closed after its bounded transaction returns. */
static bool reserve_revoke_io(bool needed)
{
    if (!needed) {
        return false;
    }
    const bool locked = take_io_lock();
    if (!locked) {
        (void)set_oe_serialized(true);
    }
    return locked;
}

static bool force_safe_stop_io_locked(void)
{
    if (xSemaphoreTake(s_oe_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    const bool ok = qw_actuator_pca9685.force_safe_stop();
    xSemaphoreGive(s_oe_mutex);
    return ok;
}

/* Called with the I/O lock held. This closes the software gate before a new
 * command can acquire that same lock after the hardware stop transaction. */
static void mark_hardware_closed_io_locked(bool safe_stop_ok)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_armed || s_open_in_progress) {
        s_authorization_epoch++;
    }
    s_actuator = &qw_actuator_null;
    s_armed = false;
    s_safe_stop_pending = !safe_stop_ok;
    if (!safe_stop_ok) {
        s_actuator_fault = true;
    }
    s_open_in_progress = false;
    s_abort_open = false;
    s_transition_in_progress = false;
    xSemaphoreGive(s_mutex);
}

static qw_motion_frame_t hold_frame(void)
{
    qw_motion_frame_t frame = {0};
    frame.command_mask = 0x0FU;
    for (uint8_t leg = 0; leg < QW_MOTION_MAX_LEGS; leg++) {
        frame.legs[leg] = (leg_command_t){LEG_CMD_HOLD_SUPPORT, 0.0f};
    }
    return frame;
}

static bool open_still_authorized(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool authorized = !s_abort_open && gate_should_be_open_locked();
    xSemaphoreGive(s_mutex);
    return authorized;
}

static bool valid_motion_frame(const qw_motion_frame_t *frame)
{
    if (frame == NULL || frame->command_mask == 0U ||
        (frame->command_mask & (uint8_t)~0x0FU) != 0U) {
        return false;
    }
    if ((frame->joint_target_mask & (uint8_t)~frame->command_mask) != 0U) {
        return false;
    }
    for (uint8_t i = 0; i < QW_MOTION_MAX_LEGS; i++) {
        if ((frame->command_mask & (uint8_t)(1U << i)) == 0U) {
            continue;
        }
        const leg_command_t *command = &frame->legs[i];
        if (command->mode > LEG_CMD_SAFE_STOP ||
            !isfinite(command->progress)) {
            return false;
        }
        if ((command->mode == LEG_CMD_LIFT ||
             command->mode == LEG_CMD_SWING) &&
            (command->progress < 0.0f || command->progress > 1.0f)) {
            return false;
        }
        if (command->mode == LEG_CMD_SEARCH && command->progress < 0.0f) {
            return false;
        }
        if ((frame->joint_target_mask & (uint8_t)(1U << i)) != 0U) {
            const qw_motion_joint_target_t *joint = &frame->joints[i];
            if (!joint->valid || !isfinite(joint->hip_rad) ||
                !isfinite(joint->knee_rad)) {
                return false;
            }
        }
    }
    return true;
}

static void close_hardware(const char *reason)
{
    bool safe_stop_ok = false;
    if (take_io_lock()) {
        safe_stop_ok = force_safe_stop_io_locked();
        mark_hardware_closed_io_locked(safe_stop_ok);
        xSemaphoreGive(s_io_mutex);
    } else {
        /* The normal path is serialized. If a slow bus operation already
         * owns the I/O lock, assert the independent hardware disable line
         * directly, then make a second bounded attempt to complete the I2C
         * rest/SLEEP sequence after the current transaction unwinds. */
        (void)set_oe_serialized(true);
        if (take_io_lock()) {
            safe_stop_ok = force_safe_stop_io_locked();
            mark_hardware_closed_io_locked(safe_stop_ok);
            xSemaphoreGive(s_io_mutex);
        } else {
            /* /OE is already the independent physical stop. Keep the
             * pending flag set because rest/SLEEP were not confirmed. */
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            if (s_armed || s_open_in_progress) {
                s_authorization_epoch++;
            }
            s_actuator = &qw_actuator_null;
            s_armed = false;
            s_safe_stop_pending = true;
            s_actuator_fault = true;
            s_open_in_progress = false;
            s_abort_open = false;
            s_transition_in_progress = false;
            xSemaphoreGive(s_mutex);
        }
    }
    qw_uart_log_safety_gate_blocked(reason);
}

static void open_hardware(void)
{
    const qw_motion_frame_t hold = hold_frame();
    bool opened = false;
    if (open_still_authorized() && take_io_lock()) {
        /* The first check may have happened while waiting for I/O. Recheck
         * after acquiring the transaction lock before waking/writing. */
        const bool frame_ready = open_still_authorized() &&
                                 qw_actuator_pca9685.command_frame(&hold);
        if (frame_ready && xSemaphoreTake(s_oe_mutex, portMAX_DELAY) == pdTRUE) {
            /* This check and /OE LOW are ordered against every revoke path. */
            opened = open_still_authorized() &&
                     qw_actuator_pca9685.set_oe(false);
            xSemaphoreGive(s_oe_mutex);
        }
        if (!opened) {
            (void)set_oe_serialized(true);
        }
        xSemaphoreGive(s_io_mutex);
    }
    if (!opened) {
        (void)set_oe_serialized(true);
    }

    bool still_authorized;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    still_authorized = opened && !s_abort_open && gate_should_be_open_locked();
    if (still_authorized) {
        s_actuator = &qw_actuator_pca9685;
        s_armed = true;
        s_transition_in_progress = false;
    } else {
        s_actuator = &qw_actuator_null;
        s_armed = false;
        if (!opened) {
            s_actuator_fault = true;
        }
        s_transition_in_progress = false;
        s_open_in_progress = false;
        s_abort_open = false;
    }
    xSemaphoreGive(s_mutex);

    if (still_authorized) {
        qw_uart_log_safety_gate_opened();
    } else {
        close_hardware(opened ? "authorization_changed" :
                                      "actuator_arm_failed");
    }
}

static void reconcile_gate(void)
{
    for (unsigned attempt = 0; attempt < 2U; attempt++) {
        bool abort_open = false;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        qw_gate_action_t action = plan_action_locked();
        const char *reason = action == QW_GATE_ACTION_CLOSE
                                 ? block_reason_locked()
                                 : "actuator_arm_failed";
        xSemaphoreGive(s_mutex);
        if (action == QW_GATE_ACTION_NONE) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            abort_open = s_abort_open;
            xSemaphoreGive(s_mutex);
            if (abort_open) {
                /* /OE is independent of the I2C transaction lock. Assert it
                 * immediately while an opening bus transaction unwinds. */
                if (!set_oe_serialized(true)) {
                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                    s_actuator_fault = true;
                    xSemaphoreGive(s_mutex);
                }
            }
            return;
        }
        if (action == QW_GATE_ACTION_OPEN) {
            open_hardware();
        } else {
            close_hardware(reason);
        }
    }
}

void safety_gate_init(void)
{
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_storage);
    s_io_mutex = xSemaphoreCreateMutexStatic(&s_io_mutex_storage);
    s_oe_mutex = xSemaphoreCreateMutexStatic(&s_oe_mutex_storage);
#if defined(CONFIG_NIULAI_MODE_BLE_ONLY)
    s_stage = QW_STAGE_BLE_ONLY;
#elif defined(CONFIG_NIULAI_MODE_BLE_VERIFIED)
    s_stage = QW_STAGE_BLE_VERIFIED;
#else
    s_stage = QW_STAGE_ACTUATION_ENABLED;
#endif
    s_actuator = &qw_actuator_null;
    s_armed = false;
    s_actuator_ready = false;
    s_control_ready = false;
    s_contact_ready = false;
    s_failsafe = false;
    s_fall_detected = false;
    s_motion_fault = false;
    s_actuator_fault = false;
    s_transition_in_progress = false;
    s_open_in_progress = false;
    s_abort_open = false;
    s_stop_latched = false;
    s_safe_stop_pending = false;
    s_authorization_epoch = 0U;
    s_acceptance_ticks = 0;

    if (s_stage != QW_STAGE_BLE_ONLY) {
        /* init leaves /OE high and the chip in SLEEP; never armed here */
        s_actuator_ready = qw_actuator_pca9685.init();
        if (!s_actuator_ready) {
            qw_uart_log_safety_gate_init_failed();
        }
    }
}

qw_stage_t safety_gate_stage(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    qw_stage_t stage = s_stage;
    xSemaphoreGive(s_mutex);
    return stage;
}

bool safety_gate_armed(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool armed = s_armed;
    xSemaphoreGive(s_mutex);
    return armed;
}

void safety_gate_notify_motion_state(qw_motion_state_code_t state)
{
    const bool motion_fault = state == QW_MOTION_STATE_FAULT;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool revoking = motion_fault && !s_motion_fault;
    xSemaphoreGive(s_mutex);
    const bool io_locked = reserve_revoke_io(revoking);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (motion_fault && !s_motion_fault) {
        s_authorization_epoch++;
        s_abort_open = true;
    }
    s_motion_fault = motion_fault;
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    reconcile_gate();
}

void safety_gate_notify_control_ready(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_control_ready = true;
    xSemaphoreGive(s_mutex);
    reconcile_gate();
}

void safety_gate_notify_contact_ready(bool ready)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool revoking = !ready && s_contact_ready;
    xSemaphoreGive(s_mutex);
    const bool io_locked = reserve_revoke_io(revoking);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!ready && s_contact_ready) {
        s_authorization_epoch++;
        s_abort_open = true;
    }
    s_contact_ready = ready;
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    reconcile_gate();
}

bool safety_gate_apply(const qw_motion_frame_t *frame)
{
    if (frame == NULL) {
        safety_gate_force_safe_stop("null_motion_frame");
        return false;
    }
    if (!valid_motion_frame(frame)) {
        safety_gate_force_safe_stop("invalid_motion_frame");
        return false;
    }
    for (uint8_t i = 0; i < QW_MOTION_MAX_LEGS; i++) {
        if ((frame->command_mask & (uint8_t)(1U << i)) != 0U &&
            frame->legs[i].mode == LEG_CMD_SAFE_STOP) {
            safety_gate_force_safe_stop("safe_stop");
            return false;
        }
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const leg_actuator_t *actuator = s_armed ? s_actuator : &qw_actuator_null;
    xSemaphoreGive(s_mutex);
    if (actuator == &qw_actuator_null || frame->command_mask == 0U) {
        return false;
    }

    bool ok = false;
    if (take_io_lock()) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        const uint32_t command_epoch = s_authorization_epoch;
        bool still_armed = s_armed && s_actuator == actuator &&
                           gate_should_be_open_locked();
        xSemaphoreGive(s_mutex);
        if (still_armed) {
            ok = actuator->command_frame(frame);
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            ok = qw_safety_gate_command_commit_allowed(
                ok, s_armed, s_actuator == actuator,
                gate_should_be_open_locked(), command_epoch,
                s_authorization_epoch);
            xSemaphoreGive(s_mutex);
        }
        xSemaphoreGive(s_io_mutex);
    }
    if (!ok) {
        safety_gate_force_safe_stop("actuator_write");
        return false;
    }
    return true;
}

void safety_gate_acceptance_feed(bool fresh_packet, bool failsafe_active)
{
    /* Reserve I/O for every authorization-breaking watchdog sample, not only
     * when the snapshot currently says armed. The open transition can start
     * after a snapshot, so this lock is what serializes that transition with
     * the revoke and lets the epoch abort it before /OE can be lowered. */
    const bool needs_io = failsafe_active || !fresh_packet;
    const bool io_locked = reserve_revoke_io(needs_io);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if ((failsafe_active || !fresh_packet) &&
        (s_armed || s_open_in_progress)) {
        s_authorization_epoch++;
        s_abort_open = true;
    }
    s_failsafe = failsafe_active;
    if (fresh_packet) {
        const int required = required_acceptance_ticks();
        if (s_acceptance_ticks < required) {
            s_acceptance_ticks++;
        }
    } else {
        s_acceptance_ticks = 0;
    }
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    reconcile_gate();
}

void safety_gate_on_failsafe(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool needs_io = !s_failsafe;
    xSemaphoreGive(s_mutex);
    const bool io_locked = reserve_revoke_io(needs_io);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_failsafe) {
        s_authorization_epoch++;
        s_abort_open = true;
    }
    s_failsafe = true;
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    reconcile_gate();
}

void safety_gate_notify_fall(bool detected)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool changed = s_fall_detected != detected;
    xSemaphoreGive(s_mutex);
    const bool io_locked = reserve_revoke_io(detected && changed);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (changed && detected) {
        s_authorization_epoch++;
        s_abort_open = true;
        s_acceptance_ticks = 0;
    }
    s_fall_detected = detected;
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    if (changed) {
        reconcile_gate();
    }
}

void safety_gate_on_input_disconnect(void)
{
    /* A disconnect can abort an in-flight open even before s_armed is set. */
    const bool io_locked = reserve_revoke_io(true);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_failsafe || s_armed || s_open_in_progress) {
        s_authorization_epoch++;
        s_abort_open = true;
    }
    s_acceptance_ticks = 0;
    s_failsafe = true;
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    reconcile_gate();
}

void safety_gate_force_safe_stop(const char *reason)
{
    const bool io_locked = reserve_revoke_io(true);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_authorization_epoch++;
    s_armed = false;
    s_actuator = &qw_actuator_null;
    s_acceptance_ticks = 0;
    s_stop_latched = true;
    s_transition_in_progress = true;
    s_open_in_progress = false;
    s_abort_open = true;
    s_safe_stop_pending = true;
    xSemaphoreGive(s_mutex);
    if (io_locked) {
        xSemaphoreGive(s_io_mutex);
    }
    close_hardware(reason == NULL ? "safe_stop" : reason);
}

void safety_gate_clear_stop_latch(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const ble_input_gate_snapshot_t input = ble_input_gate_snapshot();
    const bool can_clear =
        s_stop_latched && !s_failsafe && !s_motion_fault &&
        !s_actuator_fault && s_actuator_ready && s_control_ready &&
        s_contact_ready && input.link_secure && input.input.leg_mask == 0U;
    if (can_clear) {
        s_stop_latched = false;
        s_acceptance_ticks = 0;
    }
    xSemaphoreGive(s_mutex);
    if (can_clear) {
        reconcile_gate();
    }
}

bool safety_gate_acceptance_ok(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ok = acceptance_ok_locked();
    xSemaphoreGive(s_mutex);
    return ok;
}

int safety_gate_acceptance_ticks(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int ticks = s_acceptance_ticks;
    xSemaphoreGive(s_mutex);
    return ticks;
}

bool safety_gate_failsafe(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool failsafe = s_failsafe;
    xSemaphoreGive(s_mutex);
    return failsafe;
}
