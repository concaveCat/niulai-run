#include "sdkconfig.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "servo_zero_leg_math.h"
#include "servo_zero_math.h"
#include "servo_zero_quad_demo.h"
#include "servo_zero_trace.h"
#include "servo_zero_tcp_diag.h"
#include "servo_uart_protocol.h"

#define PCA9685_OSCILLATOR_HZ 25000000u
#define PCA9685_REG_MODE1 0x00u
#define PCA9685_REG_MODE2 0x01u
#define PCA9685_REG_PRESCALE 0xfeu
#define PCA9685_MODE1_SLEEP 0x10u
#define PCA9685_MODE1_RESTART 0x80u
#define PCA9685_MODE1_AI 0x20u
#define PCA9685_MODE2_OUTDRV 0x04u
#define SERVO_ZERO_TEST_MIN_US 300
#define SERVO_ZERO_TEST_MAX_US 3000
#define SERVO_ZERO_I2C_MAX_ATTEMPTS 4u
#define SERVO_ZERO_I2C_RETRY_DELAY_US 1000u
#define SERVO_ZERO_CHANNELS 16
#define SERVO_ZERO_ACTIVE_CHANNELS 8
#define SERVO_ZERO_DEFAULT_US 1500
#define SERVO_ZERO_ANGLE_US_PER_DEG (1000.0f / 180.0f)
#define SERVO_ZERO_MOTION_PERIOD_MS SERVO_ZERO_KNEE_SWING_UPDATE_MS
#define SERVO_ZERO_RF_MATH_PI 3.14159265358979323846f
#define SERVO_ZERO_RF_MATH_RAD_PER_DEG \
    (SERVO_ZERO_RF_MATH_PI / 180.0f)
#define SERVO_ZERO_RF_MATH_LIFT_MM 5.0f
#define SERVO_ZERO_RF_MATH_REST_MS 400u
#define SERVO_ZERO_RF_MATH_MOVE_MS 1800u
#define SERVO_ZERO_RF_SEQUENCE_NEUTRAL_MS 100u
#define SERVO_ZERO_RF_SEQUENCE_REST_WAIT_MS 1000u
#define SERVO_ZERO_RF_SEQUENCE_LIFT_WAIT_MS 1000u
#define SERVO_ZERO_RF_SEQUENCE_LIFT_DEG 45.0f
#define SERVO_ZERO_RF_SEQUENCE_HIP_DEG 45.0f
#define SERVO_ZERO_RF_SEQUENCE_LOWER_DEG 90.0f
#define SERVO_ZERO_RF_SEQUENCE_PHASE_NEUTRAL 0u
#define SERVO_ZERO_RF_SEQUENCE_PHASE_REST 1u
#define SERVO_ZERO_RF_SEQUENCE_PHASE_LIFT 2u
#define SERVO_ZERO_RF_SEQUENCE_PHASE_FINAL 3u
#define SERVO_ZERO_MOTIONLEG_LIFT_DEG 45.0f
/* Local CH1 slope is -10.9 us/deg; round the requested 45 deg to -491 us. */
#define SERVO_ZERO_MOTIONLEG_LIFT_OFFSET_US (-491)
#define SERVO_ZERO_MOTIONLEG_LIFT_MS 100u
#define SERVO_ZERO_MOTIONLEG_WAIT_MS 120u
#define SERVO_ZERO_MOTIONLEG_SWING_MS 240u
#define SERVO_ZERO_MOTIONLEG_DEMO_SWING_MS 1800u
#define SERVO_ZERO_MOTIONLEG_DEMO_SWING_DEG 45.0f
#define SERVO_ZERO_MOTIONLEG_MIN_SWING_MS 200u
#define SERVO_ZERO_MOTIONLEG_MAX_SWING_MS 5000u
#define SERVO_ZERO_MOTIONLEG_TOUCHDOWN_MS 120u
#define SERVO_ZERO_MOTIONLEG_DEFAULT_SWING_DEG 10.0f
#define SERVO_ZERO_MOTIONLEG_MAX_SWING_DEG 60.0f
#define SERVO_ZERO_VERTICAL_SWEEP_MS 4000u
#define SERVO_ZERO_VERTICAL_SHANK_RAD \
    (-SERVO_ZERO_RF_MATH_PI / 2.0f)
#define SERVO_ZERO_RF_MATH_HIP_US_PER_RAD \
    (14.6f * 180.0f / SERVO_ZERO_RF_MATH_PI)
#define SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD \
    (-10.9f * 180.0f / SERVO_ZERO_RF_MATH_PI)

static const char *TAG = "servo_zero";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_pca;
static SemaphoreHandle_t s_i2c_mutex;
static volatile bool s_outputs_enabled;
static volatile bool s_rf_step_active;
static volatile bool s_rf_math_active;
static volatile bool s_rest_pose_active;
static volatile bool s_rest_pose_reached;
static volatile bool s_knee_swing_active;
static volatile bool s_ch1_cosine_sweep_active;
static volatile bool s_motionleg_active;
static volatile bool s_motionleg_released;
static volatile bool s_rf_sequence_active;
static volatile bool s_vertical_sweep_active;
static volatile bool s_quad_demo_active;
static volatile bool s_quad_demo_holding;
static volatile bool s_quad_demo_live_mode;
static volatile bool s_quad_demo_rest_ready;
static volatile bool s_quad_press_active;
static uint8_t s_quad_press_mask;
static uint8_t s_quad_swing_mask;
static bool s_quad_hip_limit_reached[SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
static uint8_t s_quad_soft_limit_leg_mask;
static servo_zero_quad_demo_live_state_t s_quad_live_state;
static volatile bool s_pulse_dirty;
static uint16_t s_pulse_us[SERVO_ZERO_CHANNELS];
static uint8_t s_selected_channel;
static int64_t s_rf_step_started_us;
static int64_t s_rf_math_started_us;
static int64_t s_rest_pose_started_us;
static int64_t s_knee_swing_started_us;
static int64_t s_ch1_cosine_sweep_started_us;
static int64_t s_motionleg_started_us;
static int64_t s_rf_sequence_started_us;
static int64_t s_vertical_sweep_started_us;
static int64_t s_quad_demo_started_us;
static uint8_t s_rf_sequence_phase;
static uint32_t s_motionleg_release_ms;
static uint16_t s_rf_step_start_hip_us;
static uint16_t s_rf_step_start_knee_us;
static uint8_t s_rf_step_amplitude_percent =
    SERVO_ZERO_AMPLITUDE_MAX_PERCENT;
static servo_zero_leg_flow_plan_t s_rf_math_plan;
static servo_zero_leg_pulse_map_t s_rf_math_pulse_map;
static uint8_t s_rest_pose_amplitude_percent =
    SERVO_ZERO_AMPLITUDE_MAX_PERCENT;
static bool s_rest_pose_split_frames;
static uint8_t s_rest_pose_split_phase;
static uint8_t s_knee_swing_amplitude_percent =
    SERVO_ZERO_AMPLITUDE_MAX_PERCENT;
static uint16_t s_ch1_cosine_sweep_initial_us;
static float s_motionleg_swing_deg;
static uint32_t s_motionleg_swing_ms;
static servo_zero_motionleg_plan_t s_motionleg_plan;
static servo_zero_leg_pulse_map_t s_motionleg_pulse_map;
static servo_zero_leg_vertical_sweep_plan_t s_vertical_sweep_plan;
static servo_zero_leg_pulse_map_t s_vertical_sweep_pulse_map;
static servo_zero_leg_angles_t s_vertical_sweep_rest_angles;
static servo_zero_quad_demo_plan_t s_quad_demo_plan;
static servo_zero_leg_point_t s_quad_demo_foot[
    SERVO_ZERO_QUAD_DEMO_LEG_COUNT];
static uint16_t s_vertical_sweep_initial_us[2];
static uint16_t s_vertical_sweep_positive_us;
static uint16_t s_vertical_sweep_negative_us;
static uint16_t s_rf_sequence_rest_us[SERVO_ZERO_ACTIVE_CHANNELS];
static uint16_t s_rf_sequence_lift_us[2];
static uint16_t s_rf_sequence_final_us[2];
static uint8_t s_uart_target_mask;
static uint8_t s_rest_pose_channel_mask = 0xffu;
static bool s_rest_pose_pseudo_sync;
static volatile bool s_trace_e2e_running;
static volatile bool s_trace_e2e_dry_run;
static volatile bool s_trace_legacy_running;
static volatile uint32_t s_trace_legacy_record_id =
    SERVO_ZERO_TRACE_INVALID_ID;
static volatile bool s_trace_local_dry_run;
static volatile bool s_trace_target_pending;
static volatile uint32_t s_trace_target_sequence;
static volatile uint16_t s_trace_target_pulse_us;
static volatile uint32_t s_trace_target_record_id =
    SERVO_ZERO_TRACE_INVALID_ID;
static portMUX_TYPE s_trace_command_lock = portMUX_INITIALIZER_UNLOCKED;
typedef struct {
    uint32_t logical_transactions;
    uint32_t physical_attempts;
    uint32_t successful_transactions;
    uint32_t nack_errors;
    uint32_t timeout_errors;
    uint32_t other_errors;
    uint32_t retries;
    uint32_t recovered_after_retry;
    uint32_t terminal_failures;
} servo_i2c_stats_t;

static servo_i2c_stats_t s_i2c_stats;
static const char *const s_channel_names[SERVO_ZERO_ACTIVE_CHANNELS] = {
    "rf_hip", "rf_knee", "lf_hip", "lf_knee",
    "rr_hip", "rr_knee", "lr_hip", "lr_knee",
};

static uint32_t take_legacy_trace_record(void);
static void process_command(char *line);

static void disable_outputs(void)
{
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 1);
}

static esp_err_t transmit_pca(const uint8_t *bytes, size_t size,
                              const char *operation)
{
    esp_err_t err = ESP_FAIL;
    s_i2c_stats.logical_transactions++;
    for (uint8_t attempt = 1u; attempt <= SERVO_ZERO_I2C_MAX_ATTEMPTS;
         ++attempt) {
        s_i2c_stats.physical_attempts++;
        servo_zero_trace_i2c_begin();
        err = i2c_master_transmit(s_pca, bytes, size, 100);
        servo_zero_trace_i2c_end((int32_t)err);
        if (err == ESP_OK) {
            s_i2c_stats.successful_transactions++;
            if (attempt > 1u) {
                s_i2c_stats.recovered_after_retry++;
            }
            return ESP_OK;
        }
        if (err == ESP_ERR_INVALID_STATE) {
            s_i2c_stats.nack_errors++;
        } else if (err == ESP_ERR_TIMEOUT) {
            s_i2c_stats.timeout_errors++;
            ESP_LOGE(TAG,
                     "%s I2C timeout snapshot: SDA=%d SCL=%d",
                     operation,
                     gpio_get_level(CONFIG_SERVO_ZERO_SDA_GPIO),
                     gpio_get_level(CONFIG_SERVO_ZERO_SCL_GPIO));
        } else {
            s_i2c_stats.other_errors++;
        }
        if (attempt < SERVO_ZERO_I2C_MAX_ATTEMPTS) {
            s_i2c_stats.retries++;
            ESP_LOGW(TAG,
                     "%s I2C failed on attempt %u/%u: %s (0x%x); retrying after %u us",
                     operation, attempt, SERVO_ZERO_I2C_MAX_ATTEMPTS,
                     esp_err_to_name(err), err,
                     SERVO_ZERO_I2C_RETRY_DELAY_US);
            esp_rom_delay_us(SERVO_ZERO_I2C_RETRY_DELAY_US);
        }
    }
    if (err != ESP_OK) {
        s_i2c_stats.terminal_failures++;
        ESP_LOGW(TAG, "%s I2C failed after %u attempts: %s (0x%x)",
                 operation, SERVO_ZERO_I2C_MAX_ATTEMPTS,
                 esp_err_to_name(err), err);
    }
    return err;
}

static void reset_i2c_stats(void)
{
    memset(&s_i2c_stats, 0, sizeof(s_i2c_stats));
    ESP_LOGW(TAG, "I2C stats reset");
}

static void print_i2c_stats(void)
{
    ESP_LOGW(TAG,
             "I2C stats: logical=%u attempts=%u success=%u nack=%u timeout=%u other=%u retries=%u recovered_after_retry=%u terminal_failures=%u",
             s_i2c_stats.logical_transactions,
             s_i2c_stats.physical_attempts,
             s_i2c_stats.successful_transactions,
             s_i2c_stats.nack_errors,
             s_i2c_stats.timeout_errors,
             s_i2c_stats.other_errors,
             s_i2c_stats.retries,
             s_i2c_stats.recovered_after_retry,
             s_i2c_stats.terminal_failures);
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[] = {reg, value};
    return transmit_pca(bytes, sizeof(bytes), "register write");
}

static esp_err_t write_all_channels(void)
{
    uint8_t frame[SERVO_ZERO_ACTIVE_FRAME_BYTES];
    servo_zero_make_active_frame(s_pulse_us,
                                 CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ, frame);
    esp_err_t err = transmit_pca(frame, sizeof(frame), "active frame");
    if (err == ESP_OK) {
        s_pulse_dirty = false;
    }
    return err;
}

static esp_err_t write_active_half_frame(uint8_t first_channel)
{
    uint8_t frame[SERVO_ZERO_ACTIVE_HALF_FRAME_BYTES];
    servo_zero_make_active_half_frame(
        s_pulse_us, first_channel, CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ, frame);
    return transmit_pca(frame, sizeof(frame), "active half frame");
}

static esp_err_t write_active_with_unused_off_frame(void)
{
    uint8_t frame[SERVO_ZERO_FULL_FRAME_BYTES];
    servo_zero_make_active_with_unused_off_frame(
        s_pulse_us, CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ, frame);
    return transmit_pca(frame, sizeof(frame), "active hold atomic frame");
}

static esp_err_t write_motionleg_pair_frame(void)
{
    uint8_t frame[SERVO_ZERO_ACTIVE_PAIR_FRAME_BYTES];
    servo_zero_make_active_pair_frame(
        s_pulse_us[0], s_pulse_us[1],
        CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ, frame);
    return transmit_pca(frame, sizeof(frame), "motionleg pair frame");
}

static esp_err_t write_vertical_sweep_pair_only_frame(void)
{
    uint8_t frame[SERVO_ZERO_FULL_FRAME_BYTES];
    servo_zero_make_active_pair_only_frame(
        s_pulse_us[0], s_pulse_us[1],
        CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ, frame);
    return transmit_pca(frame, sizeof(frame),
                        "vertical sweep CH0/CH1-only frame");
}

static esp_err_t write_active_channel_frame(uint8_t channel)
{
    uint8_t frame[5];
    servo_zero_make_pulse_frame(
        channel,
        servo_zero_pulse_to_counts(
            s_pulse_us[channel], CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ),
        frame);
    return transmit_pca(frame, sizeof(frame), "single channel frame");
}

static bool start_motionleg(float swing_degrees, uint32_t swing_ms)
{
    if (!isfinite(swing_degrees) || swing_degrees < 0.0f ||
        swing_degrees > SERVO_ZERO_MOTIONLEG_MAX_SWING_DEG ||
        swing_ms < SERVO_ZERO_MOTIONLEG_MIN_SWING_MS ||
        swing_ms > SERVO_ZERO_MOTIONLEG_MAX_SWING_MS || s_outputs_enabled ||
        s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    servo_zero_leg_angles_t rest_angles;
    if (!servo_zero_leg_ik((servo_zero_leg_point_t){0.0f, -100.0f},
                           SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &rest_angles)) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    const float knee_lift_rad =
        (float)SERVO_ZERO_MOTIONLEG_LIFT_OFFSET_US /
        (-10.9f * 57.29577951308232f);
    if (!servo_zero_motionleg_make_plan(
            rest_angles, knee_lift_rad,
            swing_degrees * SERVO_ZERO_RF_MATH_RAD_PER_DEG,
            SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &s_motionleg_plan)) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    s_motionleg_pulse_map = (servo_zero_leg_pulse_map_t){
        .hip_rest_us = s_pulse_us[0],
        .knee_rest_us = s_pulse_us[1],
        .hip_us_per_rad = 14.6f * 57.29577951308232f,
        .knee_us_per_rad = -10.9f * 57.29577951308232f,
        .min_us = SERVO_ZERO_TEST_MIN_US,
        .max_us = SERVO_ZERO_TEST_MAX_US,
    };
    /* The local us map is only valid inside the configured pulse range.
     * Reject the whole nominal path before enabling /OE instead of letting a
     * larger swing discover an unreachable endpoint during motion. */
    const servo_zero_leg_pulse_pose_t lifted_pulses =
        servo_zero_leg_map_angles_to_pulses(
            s_motionleg_plan.rest_angles,
            s_motionleg_plan.lifted_angles, &s_motionleg_pulse_map);
    const servo_zero_leg_pulse_pose_t swing_pulses =
        servo_zero_leg_map_angles_to_pulses(
            s_motionleg_plan.rest_angles,
            s_motionleg_plan.swing_angles, &s_motionleg_pulse_map);
    const servo_zero_leg_pulse_pose_t touchdown_pulses =
        servo_zero_leg_map_angles_to_pulses(
            s_motionleg_plan.rest_angles,
            s_motionleg_plan.touchdown_angles, &s_motionleg_pulse_map);
    if (!lifted_pulses.valid || !swing_pulses.valid ||
        !touchdown_pulses.valid) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    esp_err_t err = write_motionleg_pair_frame();
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        return false;
    }
    s_motionleg_swing_deg = swing_degrees;
    s_motionleg_swing_ms = swing_ms;
    s_motionleg_started_us = esp_timer_get_time();
    s_motionleg_release_ms = 0u;
    s_motionleg_released = false;
    s_motionleg_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static void release_motionleg(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_motionleg_active && !s_motionleg_released) {
        const int64_t elapsed_us = esp_timer_get_time() - s_motionleg_started_us;
        uint32_t elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
        const uint32_t swing_start_ms =
            SERVO_ZERO_MOTIONLEG_LIFT_MS + SERVO_ZERO_MOTIONLEG_WAIT_MS;
        if (elapsed_ms < swing_start_ms) {
            elapsed_ms = swing_start_ms;
        }
        s_motionleg_release_ms = elapsed_ms;
        s_motionleg_released = true;
        ESP_LOGW(TAG,
                 "motionleg release at %u ms: hip frozen at swing=%0.1f deg; accumulated knee recovery active",
                 elapsed_ms, s_motionleg_swing_deg);
    }
    xSemaphoreGive(s_i2c_mutex);
}

static void abort_motionleg(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_motionleg_active = false;
        s_motionleg_released = false;
        xSemaphoreGive(s_i2c_mutex);
    }
}

static void motionleg_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_motionleg_active) {
                const int64_t elapsed_us = esp_timer_get_time() - s_motionleg_started_us;
                const uint32_t elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
                const uint32_t release_ms = s_motionleg_released
                                                ? s_motionleg_release_ms
                                                : UINT32_MAX;
                servo_zero_motionleg_pose_t pose = servo_zero_motionleg_pose(
                    elapsed_ms, release_ms, SERVO_ZERO_MOTIONLEG_LIFT_MS,
                    SERVO_ZERO_MOTIONLEG_WAIT_MS,
                    s_motionleg_swing_ms,
                    SERVO_ZERO_MOTIONLEG_TOUCHDOWN_MS, &s_motionleg_plan);
                if (!pose.valid) {
                    s_motionleg_active = false;
                    s_outputs_enabled = false;
                    disable_outputs();
                } else {
                    servo_zero_leg_pulse_pose_t pulses =
                        servo_zero_leg_map_angles_to_pulses(
                            s_motionleg_plan.rest_angles, pose.angles,
                            &s_motionleg_pulse_map);
                    if (!pulses.valid) {
                        s_motionleg_active = false;
                        s_outputs_enabled = false;
                        disable_outputs();
                        ESP_LOGE(TAG, "motionleg pulse mapping rejected; /OE HIGH");
                    } else {
                        s_pulse_us[0] = pulses.hip_us;
                        s_pulse_us[1] = pulses.knee_us;
                        if (write_motionleg_pair_frame() != ESP_OK) {
                            s_motionleg_active = false;
                            s_outputs_enabled = false;
                            disable_outputs();
                        } else if (pose.complete) {
                            s_motionleg_active = false;
                            ESP_LOGW(TAG, "motionleg touchdown complete; hip held; /OE LOW; use stop");
                        }
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static const uint8_t s_quad_demo_hip_channels[
    SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP,
        SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP,
        SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_HIP,
        SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_HIP,
    };
static const uint8_t s_quad_demo_knee_channels[
    SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_KNEE,
        SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_KNEE,
        SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_KNEE,
        SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_KNEE,
    };
/* These are explicit, replaceable candidates.  Only the RF local slopes have
 * any bench evidence; the other six joint slopes must not be called a final
 * calibration. */
static const float s_quad_demo_hip_us_per_rad[
    SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        SERVO_ZERO_RF_MATH_HIP_US_PER_RAD,
        -SERVO_ZERO_RF_MATH_HIP_US_PER_RAD,
        SERVO_ZERO_RF_MATH_HIP_US_PER_RAD,
        -SERVO_ZERO_RF_MATH_HIP_US_PER_RAD,
    };
static const float s_quad_demo_knee_us_per_rad[
    SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {
        SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD,
        -SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD,
        SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD,
        -SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD,
    };
static const char *const s_quad_demo_leg_names[
    SERVO_ZERO_QUAD_DEMO_LEG_COUNT] = {"RF", "LF", "RR", "LR"};

/*
 * Operator-confirmed, channel-specific directional stops. The generic
 * 500..2500 us parser range is only a bench transport range and must never
 * be used as the quad demo's mechanical envelope. Every entry is edited
 * independently because the four hip axes are not mechanically aligned;
 * an unset side means that no stop is currently known on that direction.
 */
static const servo_zero_quad_demo_pulse_limits_t s_quad_demo_pulse_limits = {
    .configured = true,
    .channel = {
        /* CH0 RF hip: front-leg backward stop at 1400 us. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_HIP] = {
            .has_min = true, .min_us = 1400u},
        /* CH1 RF knee: no known mechanical stop. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_RF_KNEE] = {
            .has_min = false, .has_max = false},
        /* CH2 LF hip: front-leg backward stop at 1750 us. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_HIP] = {
            .has_max = true, .max_us = 1750u},
        /* CH3 LF knee: no known mechanical stop. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_LF_KNEE] = {
            .has_min = false, .has_max = false},
        /* CH4 RR hip: rear-leg forward stop at 1940 us. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_HIP] = {
            .has_max = true, .max_us = 1940u},
        /* CH5 RR knee: no known mechanical stop. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_RR_KNEE] = {
            .has_min = false, .has_max = false},
        /* CH6 LR hip: rear-leg forward stop at 1120 us. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_HIP] = {
            .has_min = true, .min_us = 1120u},
        /* CH7 LR knee: no known mechanical stop. */
        [SERVO_ZERO_QUAD_DEMO_CHANNEL_LR_KNEE] = {
            .has_min = false, .has_max = false},
    },
};

static bool quad_demo_pulse_limits_configured(void)
{
    return s_quad_demo_pulse_limits.configured;
}

static void reset_quad_demo_foot_state(void)
{
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        s_quad_demo_foot[leg] = (servo_zero_leg_point_t){
            .x_mm = 0.0f,
            .z_mm = -100.0f,
        };
    }
}

static bool quad_demo_busy(void)
{
    return s_quad_demo_active || s_quad_demo_holding;
}

static uint8_t quad_mask_count(uint8_t mask)
{
    uint8_t count = 0u;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((mask & (uint8_t)(1u << leg)) != 0u) {
            ++count;
        }
    }
    return count;
}

static void clear_quad_press_state(void)
{
    s_quad_press_active = false;
    s_quad_press_mask = 0u;
    s_quad_swing_mask = 0u;
    s_quad_demo_live_mode = false;
    s_quad_soft_limit_leg_mask = 0u;
    s_quad_live_state = (servo_zero_quad_demo_live_state_t){0};
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        s_quad_hip_limit_reached[leg] = false;
    }
}

static void abort_quad_demo(void)
{
    if (s_i2c_mutex != NULL &&
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_quad_demo_active = false;
        s_quad_demo_holding = false;
        s_quad_demo_rest_ready = false;
        clear_quad_press_state();
        xSemaphoreGive(s_i2c_mutex);
    } else {
        s_quad_demo_active = false;
        s_quad_demo_holding = false;
        s_quad_demo_rest_ready = false;
        clear_quad_press_state();
    }
}

static uint32_t quad_elapsed_ms(int64_t started_us, int64_t now_us)
{
    if (started_us <= 0 || now_us <= started_us) {
        return 0u;
    }
    const int64_t elapsed_us = now_us - started_us;
    if (elapsed_us >= (int64_t)UINT32_MAX * 1000LL) {
        return UINT32_MAX;
    }
    return (uint32_t)(elapsed_us / 1000LL);
}

static void initialize_quad_support_safe_rear_locked(
    const servo_zero_quad_demo_plan_t *plan,
    servo_zero_quad_demo_live_state_t *state, uint8_t leg_mask)
{
    if (plan == NULL || !plan->valid || state == NULL) {
        if (state != NULL) {
            state->support_safe_rear_valid_mask = 0u;
        }
        return;
    }
    uint8_t valid_mask = state->support_safe_rear_valid_mask;
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((leg_mask & bit) == 0u) {
            continue;
        }
        /* A valid boundary belongs to the current support cycle.  A new
         * press creates a new action window, but must not silently recreate
         * the boundary for an already-landed support leg. */
        if ((valid_mask & bit) != 0u) {
            continue;
        }
        state->support_safe_rear_x_mm[leg] =
            servo_zero_quad_demo_support_safe_rear_x_mm(
                plan, leg, state->support_base_foot[leg]);
        if (isfinite(state->support_safe_rear_x_mm[leg])) {
            valid_mask |= bit;
        } else {
            valid_mask &= (uint8_t)~bit;
        }
    }
    state->support_safe_rear_valid_mask = valid_mask;
}

static void rebase_quad_support_locked(
    const servo_zero_quad_demo_pose_t *written_pose, uint8_t landed_mask)
{
    /* Rebase only after the current pose has been written.  The support
     * origins then already contain the last applied retreat, so the next
     * frame starts with zero incremental retreat in this new body frame. */
    if (written_pose == NULL || !written_pose->valid) {
        ESP_LOGE(TAG,
                 "quad support rebase skipped: current live pose is invalid; retaining previous support reference");
        return;
    }
    if (!servo_zero_quad_demo_rebase_written_pose(
            &s_quad_demo_plan, &s_quad_live_state, written_pose,
            landed_mask)) {
        ESP_LOGE(TAG, "quad support rebase failed; retaining last written output");
    }
}

static bool parse_u32(const char *text, uint32_t *value);

static bool parse_quad_demo_profile(const char *text, float *step_length_mm,
                                    float *stance_step_mm, float *lift_mm,
                                    uint32_t *lift_ms, uint32_t *swing_ms,
                                    uint32_t *search_ms,
                                    uint8_t *active_hip_scale_percent)
{
    if (text == NULL || step_length_mm == NULL || stance_step_mm == NULL ||
        lift_mm == NULL || lift_ms == NULL || swing_ms == NULL ||
        search_ms == NULL || active_hip_scale_percent == NULL) {
        return false;
    }
    char profile[64];
    const size_t length = strlen(text);
    if (length == 0u || length >= sizeof(profile)) {
        return false;
    }
    memcpy(profile, text, length + 1u);
    char *save = NULL;
    char *token = strtok_r(profile, ",", &save);
    uint32_t values[7];
    uint8_t count = 0u;
    while (token != NULL && count < 7u) {
        if (!parse_u32(token, &values[count])) {
            return false;
        }
        ++count;
        token = strtok_r(NULL, ",", &save);
    }
    if (token != NULL || (count != 4u && count != 6u && count != 7u)) {
        return false;
    }
    const bool has_explicit_motion_profile = count >= 6u;
    const uint32_t candidate_step = has_explicit_motion_profile
                                        ? values[0]
                                        : (uint32_t)SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM;
    const uint32_t candidate_lift = has_explicit_motion_profile
                                        ? values[2]
                                        : (uint32_t)SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MM;
    const uint32_t candidate_stance = has_explicit_motion_profile
                                          ? values[1]
                                          : values[0];
    const uint32_t candidate_lift_ms = has_explicit_motion_profile
                                           ? values[3]
                                           : values[1];
    const uint32_t candidate_swing_ms = has_explicit_motion_profile
                                            ? values[4]
                                            : values[2];
    const uint32_t candidate_search_ms = has_explicit_motion_profile
                                             ? values[5]
                                             : values[3];
    const uint32_t candidate_hip_scale = count == 7u ? values[6] : 100u;
    if (candidate_step == 0u ||
        candidate_step > (uint32_t)SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM ||
        candidate_stance == 0u ||
        candidate_stance > (uint32_t)SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM ||
        candidate_lift > (uint32_t)SERVO_ZERO_QUAD_DEMO_MAX_LIFT_MM ||
        candidate_lift_ms < SERVO_ZERO_QUAD_DEMO_UPDATE_MS ||
        candidate_swing_ms < SERVO_ZERO_QUAD_DEMO_UPDATE_MS ||
        candidate_search_ms < SERVO_ZERO_QUAD_DEMO_UPDATE_MS ||
        candidate_lift_ms > 2000u || candidate_swing_ms > 2000u ||
        candidate_search_ms > 2000u || candidate_hip_scale > 100u) {
        return false;
    }
    *step_length_mm = (float)candidate_step;
    *stance_step_mm = (float)candidate_stance;
    *lift_mm = (float)candidate_lift;
    *lift_ms = candidate_lift_ms;
    *swing_ms = candidate_swing_ms;
    *search_ms = candidate_search_ms;
    *active_hip_scale_percent = (uint8_t)candidate_hip_scale;
    return true;
}

static bool start_quad_demo_mask(uint8_t active_mask,
                                 const char *profile_text, bool live_mode)
{
    if (active_mask == 0u || (active_mask & (uint8_t)~0x0fu) != 0u) {
        return false;
    }
    uint8_t active_count = 0u;
    uint8_t active_leg = SERVO_ZERO_QUAD_DEMO_LEG_COUNT;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((active_mask & (uint8_t)(1u << leg)) != 0u) {
            ++active_count;
            if (active_leg == SERVO_ZERO_QUAD_DEMO_LEG_COUNT) {
                active_leg = leg;
            }
        }
    }
    if (active_count == 0u || active_count > 2u) {
        ESP_LOGW(TAG, "quad demo rejected: active mask must select 1 or 2 legs");
        return false;
    }
    if (!s_quad_demo_rest_ready) {
        ESP_LOGW(TAG,
                 "quad demo rejected: enter all-leg rest mode first with rest");
        return false;
    }
    float step_length_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_STEP_LENGTH_MM;
    float stance_step_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_STANCE_STEP_LENGTH_MM;
    float lift_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MM;
    uint32_t lift_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_MS;
    uint32_t swing_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_SWING_MS;
    uint32_t search_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_SEARCH_MS;
    uint8_t active_hip_scale_percent =
        SERVO_ZERO_QUAD_DEMO_DEFAULT_HIP_SCALE_PERCENT;
    if (profile_text != NULL &&
        !parse_quad_demo_profile(profile_text, &step_length_mm,
                                 &stance_step_mm, &lift_mm, &lift_ms,
                                 &swing_ms, &search_ms,
                                 &active_hip_scale_percent)) {
        ESP_LOGW(TAG,
                 "demo quad rejected; profile must be step,stance,lift,lift_ms,swing_ms,search_ms,hip_scale_percent (or legacy 4/6-field profile); lengths=1..%u mm, lift<=%u mm, durations=%u..2000 ms, hip scale=0..100%%",
                 (unsigned)SERVO_ZERO_QUAD_DEMO_MAX_STEP_LENGTH_MM,
                 (unsigned)SERVO_ZERO_QUAD_DEMO_MAX_LIFT_MM,
                 SERVO_ZERO_QUAD_DEMO_UPDATE_MS);
        return false;
    }
    const bool rest_pose_hold = s_rest_pose_reached && s_outputs_enabled;
    if (s_quad_demo_active ||
        (s_outputs_enabled && !s_quad_demo_holding && !rest_pose_hold) ||
        s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active ||
        s_vertical_sweep_active || s_trace_e2e_running ||
        s_trace_legacy_running || s_trace_local_dry_run ||
        s_i2c_mutex == NULL ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    const bool locked_rest_pose_hold = s_rest_pose_reached && s_outputs_enabled;
    if (s_quad_demo_active ||
        !s_quad_demo_rest_ready ||
        (s_outputs_enabled && !s_quad_demo_holding &&
         !locked_rest_pose_hold) ||
        s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active ||
        s_vertical_sweep_active || s_trace_e2e_running ||
        s_trace_legacy_running || s_trace_local_dry_run) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    if (!quad_demo_pulse_limits_configured()) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGE(TAG,
                 "quad demo rejected: per-channel mechanical-safe pulse limits are not configured; /OE remains HIGH");
        return false;
    }

    servo_zero_quad_demo_params_t params = {
        .active_leg = active_leg,
        .active_leg_mask = active_mask,
        .pulse_limits = s_quad_demo_pulse_limits,
        .step_length_mm = step_length_mm,
        .stance_step_length_mm = stance_step_mm,
        .lift_back_mm = SERVO_ZERO_QUAD_DEMO_DEFAULT_LIFT_BACK_MM,
        .lift_mm = lift_mm,
        .active_hip_scale_percent = active_hip_scale_percent,
        .settle_ms = SERVO_ZERO_QUAD_DEMO_DEFAULT_SETTLE_MS,
        .lift_ms = lift_ms,
        .swing_ms = swing_ms,
        .search_ms = search_ms,
    };
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t hip_channel = s_quad_demo_hip_channels[leg];
        const uint8_t knee_channel = s_quad_demo_knee_channels[leg];
        params.start_foot[leg] = servo_zero_quad_demo_limit_start_foot(
            s_quad_demo_foot[leg]);
        params.leg[leg] = (servo_zero_quad_demo_leg_config_t){
            .hip_channel = hip_channel,
            .knee_channel = knee_channel,
            .knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
            .pulse_map = {
                /* The current commanded pulse is the only available local
                 * reference when the previous pose is not encoder-backed. */
                .hip_rest_us = s_pulse_us[hip_channel],
                .knee_rest_us = s_pulse_us[knee_channel],
                .hip_us_per_rad = s_quad_demo_hip_us_per_rad[leg],
                .knee_us_per_rad = s_quad_demo_knee_us_per_rad[leg],
                .min_us = SERVO_ZERO_TEST_MIN_US,
                .max_us = SERVO_ZERO_TEST_MAX_US,
            },
        };
    }

    servo_zero_quad_demo_plan_t plan;
    const bool plan_built = live_mode
                                ? servo_zero_quad_demo_make_live_plan(&params,
                                                                       &plan)
                                : servo_zero_quad_demo_make_plan(&params,
                                                                  &plan);
    if (!plan_built) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGW(TAG,
                 "quad demo rejected: candidate path exceeds IK, q-to-us, or software foot limit x=%.1f..%.1f z=%.1f..%.1f; /OE remains HIGH",
                 SERVO_ZERO_QUAD_DEMO_MIN_FOOT_X_MM,
                 SERVO_ZERO_QUAD_DEMO_MAX_FOOT_X_MM,
                 SERVO_ZERO_QUAD_DEMO_MIN_FOOT_Z_MM,
                 SERVO_ZERO_QUAD_DEMO_MAX_FOOT_Z_MM);
        return false;
    }
    const servo_zero_quad_demo_live_state_t previous_live_state =
        s_quad_live_state;
    const bool carry_support_cycle = live_mode && s_quad_demo_holding;
    servo_zero_quad_demo_live_state_t initial_live_state = {
        .moving_leg_mask = active_mask,
        .held_leg_mask = active_mask,
        .search_leg_mask = 0u,
        .support_drive_reference_mm = 0.0f,
        .body_drive_floor_mm = 0.0f,
    };
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        initial_live_state.support_base_foot[leg] = plan.start_foot[leg];
        if (carry_support_cycle) {
            initial_live_state.support_safe_rear_x_mm[leg] =
                previous_live_state.support_safe_rear_x_mm[leg];
        }
    }
    if (carry_support_cycle) {
        initial_live_state.support_safe_rear_valid_mask =
            previous_live_state.support_safe_rear_valid_mask;
    }
    initialize_quad_support_safe_rear_locked(&plan, &initial_live_state,
                                             0x0fu);
    servo_zero_quad_demo_live_state_t lift_live_state = initial_live_state;
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((active_mask & (uint8_t)(1u << leg)) != 0u) {
            lift_live_state.lift_elapsed_ms[leg] = plan.lift_ms;
        }
    }
    const servo_zero_quad_demo_pose_t initial_pose =
        live_mode
            ? servo_zero_quad_demo_live_pose(&plan, &initial_live_state)
            : servo_zero_quad_demo_pose_at(0u, &plan);
    if (!initial_pose.valid) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGW(TAG,
                 "quad demo rejected: initial four-leg snapshot is invalid; /OE remains HIGH");
        return false;
    }
    if (live_mode) {
        const servo_zero_quad_demo_pose_t lift_pose =
            servo_zero_quad_demo_live_pose(&plan, &lift_live_state);
        if (!lift_pose.valid) {
            xSemaphoreGive(s_i2c_mutex);
            ESP_LOGW(TAG,
                     "quad demo rejected: knee-only lift endpoint exceeds IK or pulse limits; /OE remains HIGH");
            return false;
        }
    }
    uint8_t bad_channel = 0u;
    uint16_t bad_pulse_us = 0u;
    uint16_t bad_min_us = 0u;
    uint16_t bad_max_us = 0u;
    if (!servo_zero_quad_demo_pulses_within_limits(
            &plan, initial_pose.pulse_us, &bad_channel, &bad_pulse_us,
            &bad_min_us, &bad_max_us)) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGE(TAG,
                 "quad demo rejected: CH%u initial pulse %u us outside software limit %u..%u us; /OE remains HIGH",
                 bad_channel, bad_pulse_us, bad_min_us, bad_max_us);
        return false;
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        s_pulse_us[channel] = initial_pose.pulse_us[channel];
    }
    s_pulse_dirty = false;
    if (write_active_with_unused_off_frame() != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        s_quad_demo_rest_ready = false;
        ESP_LOGE(TAG,
                 "quad demo preload failed: /OE remains HIGH and no motion started");
        return false;
    }

    /* The all-leg rest hold is consumed by this demo, but a completed demo
     * remains eligible for the next 1..4 key without another rest transition.
     * An explicit stop or a failed motion invalidates the eligibility. */
    s_rest_pose_active = false;
    s_rest_pose_reached = false;
    s_quad_demo_plan = plan;
    clear_quad_press_state();
    if (live_mode) {
        const int64_t now_us = esp_timer_get_time();
        s_quad_press_mask = active_mask;
        s_quad_swing_mask = active_mask;
        s_quad_press_active = true;
        s_quad_demo_live_mode = true;
        s_quad_live_state = initial_live_state;
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if ((active_mask & (uint8_t)(1u << leg)) != 0u) {
                s_quad_live_state.swing_offset_mm[leg] = 0.0f;
                s_quad_live_state.swing_elapsed_ms[leg] = 0u;
            }
        }
        s_quad_demo_plan.active_leg_mask = active_mask;
        s_quad_demo_started_us = now_us;
    } else {
        s_quad_demo_started_us = esp_timer_get_time();
    }
    s_quad_demo_active = true;
    s_quad_demo_holding = false;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    ESP_LOGW(TAG,
             "HIGH RISK quad demo started: mask=0x%02x first_leg=%s; active_step=%.1f mm fixed_diagnostic_stance=%.1f mm lift_back=%.1f mm lift=%.1f mm live_speed_scale=%u%%; settle/lift/swing/search=%u/%u/%u/%u ms; candidate q->us; send stop",
             active_mask, s_quad_demo_leg_names[active_leg],
             params.step_length_mm, params.stance_step_length_mm,
             params.lift_back_mm, params.lift_mm,
             (unsigned)params.active_hip_scale_percent, params.settle_ms,
             params.lift_ms, params.swing_ms, params.search_ms);
    return true;
}

static bool start_quad_demo(uint8_t active_leg, const char *profile_text)
{
    return active_leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT &&
           start_quad_demo_mask((uint8_t)(1u << active_leg), profile_text,
                                false);
}

static bool parse_quad_mask(const char *text, uint8_t *mask_out)
{
    uint32_t value;
    if (text == NULL || mask_out == NULL || !parse_u32(text, &value) ||
        value == 0u || value > 0x0fu) {
        return false;
    }
    uint8_t count = 0u;
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((value & (uint32_t)(1u << leg)) != 0u) {
            ++count;
        }
    }
    if (count > 2u) {
        return false;
    }
    *mask_out = (uint8_t)value;
    return true;
}

static bool start_quad_press(uint8_t mask, const char *profile_text)
{
    if (mask == 0u || (mask & (uint8_t)~0x0fu) != 0u ||
        s_i2c_mutex == NULL ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    if (!s_quad_demo_rest_ready) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    const uint8_t requested_new_mask = (uint8_t)(mask & ~s_quad_press_mask);
    if (s_quad_press_active) {
        const uint8_t repressed_landing_mask =
            (uint8_t)(requested_new_mask & s_quad_swing_mask);
        if (repressed_landing_mask != 0u) {
            ESP_LOGW(TAG,
                     "quad press rejected: leg mask=0x%02x is already landing",
                     repressed_landing_mask);
            xSemaphoreGive(s_i2c_mutex);
            return false;
        }
        const uint8_t next_swing_mask =
            (uint8_t)(s_quad_swing_mask | requested_new_mask);
        if (quad_mask_count(next_swing_mask) > 2u) {
            ESP_LOGW(TAG,
                     "quad press rejected: at most 2 legs may swing together");
            xSemaphoreGive(s_i2c_mutex);
            return false;
        }
        if (requested_new_mask == 0u) {
            xSemaphoreGive(s_i2c_mutex);
            return true;
        }
        bool still_in_lift_window = true;
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if ((s_quad_press_mask & (uint8_t)(1u << leg)) != 0u &&
                s_quad_live_state.lift_elapsed_ms[leg] >=
                    s_quad_demo_plan.lift_ms) {
                still_in_lift_window = false;
                break;
            }
        }
        if (!s_quad_demo_live_mode || !still_in_lift_window) {
            xSemaphoreGive(s_i2c_mutex);
            ESP_LOGW(TAG,
                     "quad press rejected: simultaneous keys must arrive before the first leg starts hip swing (%u ms lift window)",
                     s_quad_demo_plan.lift_ms);
            return false;
        }
        /* A second key is accepted only during the knee-first lift window.
         * No support retreat has occurred yet, so changing the active mask
         * does not require a coordinate-frame rebase. */
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if ((requested_new_mask & (uint8_t)(1u << leg)) != 0u) {
                s_quad_demo_plan.start_foot[leg] = s_quad_demo_foot[leg];
                const uint8_t hip_channel = s_quad_demo_plan.leg[leg].hip_channel;
                const uint8_t knee_channel = s_quad_demo_plan.leg[leg].knee_channel;
                s_quad_demo_plan.leg[leg].pulse_map.hip_rest_us =
                    s_pulse_us[hip_channel];
                s_quad_demo_plan.leg[leg].pulse_map.knee_rest_us =
                    s_pulse_us[knee_channel];
                s_quad_hip_limit_reached[leg] = false;
                s_quad_live_state.swing_offset_mm[leg] = 0.0f;
                s_quad_live_state.lift_elapsed_ms[leg] = 0u;
                s_quad_live_state.swing_elapsed_ms[leg] = 0u;
                s_quad_live_state.search_elapsed_ms[leg] = 0u;
            }
        }
        s_quad_press_mask = (uint8_t)(s_quad_press_mask | mask);
        s_quad_swing_mask = next_swing_mask;
        s_quad_live_state.held_leg_mask = s_quad_press_mask;
        s_quad_live_state.moving_leg_mask = s_quad_swing_mask;
        s_quad_demo_plan.active_leg_mask = next_swing_mask;
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGW(TAG,
                 "quad press added: key_mask=0x%02x held_mask=0x%02x swing_mask=0x%02x",
                 mask, s_quad_press_mask, s_quad_swing_mask);
        return true;
    }
    /* A previous trajectory may still be completing a fixed landing. */
    if (s_quad_demo_active) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    xSemaphoreGive(s_i2c_mutex);
    if (!start_quad_demo_mask(mask, profile_text, true)) {
        return false;
    }
    ESP_LOGW(TAG,
             "quad press started: key_mask=0x%02x; knee lift=%u ms then bounded horizontal foot swing (body drive=%u%% of active swing; independent support reserve); release each key independently",
             mask, (unsigned)s_quad_demo_plan.lift_ms,
             (unsigned)SERVO_ZERO_QUAD_DEMO_DEFAULT_BODY_DRIVE_PERCENT);
    return true;
}

static bool release_quad_press(uint8_t mask)
{
    if (mask == 0u || (mask & (uint8_t)~0x0fu) != 0u ||
        s_i2c_mutex == NULL ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    if (!s_quad_press_active || (mask & s_quad_press_mask) == 0u) {
        /* Key-up can be duplicated or its first UART response can be lost.
         * Treat a release already latched for this trajectory as idempotent,
         * so the host never gets stuck with a phantom held key. */
        if ((mask & s_quad_swing_mask) != 0u || s_quad_demo_holding) {
            ESP_LOGW(TAG,
                     "quad key release already applied: key_mask=0x%02x; fixed landing continues",
                     mask);
            xSemaphoreGive(s_i2c_mutex);
            return true;
        }
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    const uint8_t released_mask = (uint8_t)(mask & s_quad_press_mask);
    if (s_quad_demo_live_mode) {
        const servo_zero_quad_demo_pose_t current_pose =
            servo_zero_quad_demo_live_pose(&s_quad_demo_plan,
                                           &s_quad_live_state);
        if (!current_pose.valid) {
            xSemaphoreGive(s_i2c_mutex);
            return false;
        }
        for (uint8_t leg = 0u;
             leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
            if ((released_mask & (uint8_t)(1u << leg)) == 0u) {
                continue;
            }
            s_quad_live_state.release_foot[leg] = current_pose.foot[leg];
            s_quad_live_state.release_angles[leg] = current_pose.angles[leg];
            s_quad_live_state.swing_offset_mm[leg] =
                current_pose.accepted_swing_offset_mm[leg];
            s_quad_live_state.search_elapsed_ms[leg] = 0u;
        }
        /* The active swing target is now fixed for SEARCH.  Keep the body
         * translation that was already paid for; removing the released leg
         * from held_leg_mask must not make the three supports jump forward. */
        if (current_pose.body_drive_mm >
            s_quad_live_state.body_drive_floor_mm) {
            s_quad_live_state.body_drive_floor_mm =
                current_pose.body_drive_mm;
        }
    }
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        if ((released_mask & (uint8_t)(1u << leg)) != 0u) {
            s_quad_live_state.held_leg_mask &=
                (uint8_t)~(1u << leg);
            s_quad_live_state.search_leg_mask |= (uint8_t)(1u << leg);
        }
    }
    s_quad_press_mask = (uint8_t)(s_quad_press_mask & ~released_mask);
    s_quad_press_active = s_quad_press_mask != 0u;
    s_quad_live_state.moving_leg_mask = s_quad_swing_mask;
    s_quad_demo_plan.active_leg_mask = s_quad_swing_mask;
    xSemaphoreGive(s_i2c_mutex);
    ESP_LOGW(TAG,
             "quad key release: key_mask=0x%02x released=0x%02x held_mask=0x%02x; fixed-timing foot landing starts",
             mask, released_mask, s_quad_press_mask);
    return true;
}

static void fail_quad_demo_locked(void)
{
    s_quad_demo_active = false;
    s_quad_demo_holding = false;
    s_quad_demo_rest_ready = false;
    clear_quad_press_state();
    s_outputs_enabled = false;
    disable_outputs();
}

static void update_quad_live_state_locked(void)
{
    s_quad_live_state.moving_leg_mask = s_quad_swing_mask;
    s_quad_live_state.held_leg_mask = s_quad_press_mask;
    s_quad_live_state.search_leg_mask =
        (uint8_t)(s_quad_swing_mask & ~s_quad_press_mask);
    s_quad_demo_plan.active_leg_mask = s_quad_swing_mask;
    for (uint8_t leg = 0u;
         leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        const uint8_t bit = (uint8_t)(1u << leg);
        if ((s_quad_press_mask & bit) != 0u) {
            uint32_t lift_elapsed_ms =
                s_quad_live_state.lift_elapsed_ms[leg];
            if (lift_elapsed_ms < s_quad_demo_plan.lift_ms) {
                const uint32_t remaining =
                    s_quad_demo_plan.lift_ms - lift_elapsed_ms;
                lift_elapsed_ms += remaining < SERVO_ZERO_QUAD_DEMO_UPDATE_MS
                                       ? remaining
                                       : SERVO_ZERO_QUAD_DEMO_UPDATE_MS;
                s_quad_live_state.lift_elapsed_ms[leg] = lift_elapsed_ms;
            } else if (!s_quad_hip_limit_reached[leg]) {
                uint32_t swing_elapsed_ms =
                    s_quad_live_state.swing_elapsed_ms[leg];
                const uint32_t remaining =
                    swing_elapsed_ms < s_quad_demo_plan.swing_ms
                        ? s_quad_demo_plan.swing_ms - swing_elapsed_ms
                        : 0u;
                swing_elapsed_ms +=
                    remaining < SERVO_ZERO_QUAD_DEMO_UPDATE_MS
                        ? remaining
                        : SERVO_ZERO_QUAD_DEMO_UPDATE_MS;
                s_quad_live_state.swing_elapsed_ms[leg] = swing_elapsed_ms;
                s_quad_live_state.lift_elapsed_ms[leg] =
                    s_quad_demo_plan.lift_ms + swing_elapsed_ms;
                /* swing_ms is the time to reach this step's bounded target,
                 * not an instruction to keep growing without limit. */
                const float requested_offset =
                    (float)swing_elapsed_ms *
                    s_quad_demo_plan.step_length_mm /
                    (float)s_quad_demo_plan.swing_ms;
                s_quad_live_state.swing_offset_mm[leg] =
                    requested_offset >
                            s_quad_demo_plan.leg_step_length_mm[leg]
                        ? s_quad_demo_plan.leg_step_length_mm[leg]
                        : requested_offset;
            }
        }
        if ((s_quad_live_state.search_leg_mask & bit) != 0u) {
            /* SEARCH is a commanded-output trajectory, not a wall-clock
             * animation.  Advance by one logical frame so I2C retries or a
             * delayed task cannot skip intermediate knee commands and then
             * trip the frame-step guard while catching up. */
            const uint32_t elapsed =
                s_quad_live_state.search_elapsed_ms[leg];
            const uint32_t remaining =
                elapsed < s_quad_demo_plan.search_ms
                    ? s_quad_demo_plan.search_ms - elapsed
                    : 0u;
            s_quad_live_state.search_elapsed_ms[leg] =
                elapsed +
                (remaining < SERVO_ZERO_QUAD_DEMO_UPDATE_MS
                     ? remaining
                     : SERVO_ZERO_QUAD_DEMO_UPDATE_MS);
        }
    }
}

static bool write_quad_pose_locked(
    const servo_zero_quad_demo_pose_t *pose,
    uint16_t previous_pulse[SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT])
{
    if (pose == NULL || previous_pulse == NULL || !pose->valid) {
        return false;
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        previous_pulse[channel] = s_pulse_us[channel];
    }

    uint8_t bad_channel = 0u;
    uint16_t bad_pulse_us = 0u;
    uint16_t bad_min_us = 0u;
    uint16_t bad_max_us = 0u;
    const bool pulses_safe = servo_zero_quad_demo_pulses_within_limits(
        &s_quad_demo_plan, pose->pulse_us, &bad_channel, &bad_pulse_us,
        &bad_min_us, &bad_max_us);
    if (!pulses_safe) {
        ESP_LOGE(TAG,
                 "quad demo pulse limit violation: CH%u requested %u us, allowed %u..%u us",
                 bad_channel, bad_pulse_us, bad_min_us, bad_max_us);
        return false;
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        const uint16_t current = pose->pulse_us[channel];
        const uint16_t previous = previous_pulse[channel];
        const uint16_t delta = current >= previous
                                   ? current - previous
                                   : previous - current;
        if (delta > SERVO_ZERO_QUAD_DEMO_MAX_PULSE_STEP_US) {
            ESP_LOGE(TAG,
                     "quad demo frame step too large: CH%u %u->%u us",
                     channel, previous, current);
            return false;
        }
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        s_pulse_us[channel] = pose->pulse_us[channel];
    }
    if (write_active_with_unused_off_frame() == ESP_OK) {
        return true;
    }
    for (uint8_t channel = 0u;
         channel < SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT; ++channel) {
        s_pulse_us[channel] = previous_pulse[channel];
    }
    return false;
}

static void quad_demo_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        bool failed = false;
        bool completed = false;
        uint32_t elapsed_ms = 0u;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_quad_demo_active) {
                if (!s_outputs_enabled) {
                    fail_quad_demo_locked();
                } else {
                    const int64_t now_us = esp_timer_get_time();
                    elapsed_ms = quad_elapsed_ms(s_quad_demo_started_us,
                                                 now_us);
                    if (s_quad_demo_live_mode) {
                        update_quad_live_state_locked();
                    }
                    const servo_zero_quad_demo_pose_t pose =
                        s_quad_demo_live_mode
                            ? servo_zero_quad_demo_live_pose(
                                  &s_quad_demo_plan, &s_quad_live_state)
                            : servo_zero_quad_demo_pose_at(
                                  elapsed_ms, &s_quad_demo_plan);
                    if (!pose.valid) {
                        fail_quad_demo_locked();
                        failed = true;
                        if (pose.pulse_limit_violation) {
                            ESP_LOGE(TAG,
                                     "quad demo pulse limit violation: CH%u requested %u us, allowed %u..%u us",
                                     pose.limit_channel, pose.limit_pulse_us,
                                     pose.limit_min_us, pose.limit_max_us);
                        }
                    } else {
                        uint16_t previous_pulse[
                            SERVO_ZERO_QUAD_DEMO_CHANNEL_COUNT];
                        if (!write_quad_pose_locked(&pose, previous_pulse)) {
                            fail_quad_demo_locked();
                            failed = true;
                        } else {
                            s_quad_soft_limit_leg_mask |=
                                pose.soft_limit_leg_mask;
                            for (uint8_t leg = 0u;
                                 leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
                                s_quad_demo_foot[leg] = pose.foot[leg];
                            }
                            if (s_quad_demo_live_mode) {
                                const uint8_t held_mask =
                                    s_quad_live_state.held_leg_mask;
                                const uint8_t newly_limited =
                                    (uint8_t)(pose.soft_limit_leg_mask &
                                              held_mask);
                                for (uint8_t leg = 0u;
                                     leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT;
                                     ++leg) {
                                    const uint8_t bit = (uint8_t)(1u << leg);
                                    if ((newly_limited & bit) != 0u &&
                                        !s_quad_hip_limit_reached[leg]) {
                                        s_quad_hip_limit_reached[leg] = true;
                                        s_quad_live_state.swing_offset_mm[leg] =
                                            pose.accepted_swing_offset_mm[leg];
                                        ESP_LOGW(
                                            TAG,
                                            "quad leg %s reached software swing limit at x=%.1f mm; hold is latched there",
                                            s_quad_demo_leg_names[leg],
                                            pose.foot[leg].x_mm);
                                    }
                                }
                                uint8_t landed_mask = 0u;
                                for (uint8_t leg = 0u;
                                     leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT;
                                     ++leg) {
                                    const uint8_t bit = (uint8_t)(1u << leg);
                                    if ((s_quad_live_state.search_leg_mask &
                                         bit) != 0u &&
                                        s_quad_live_state.search_elapsed_ms[leg] >=
                                            s_quad_demo_plan.search_ms) {
                                        landed_mask |= bit;
                                    }
                                }
                                if (landed_mask != 0u) {
                                    s_quad_swing_mask = (uint8_t)(
                                        s_quad_swing_mask & ~landed_mask);
                                    s_quad_live_state.moving_leg_mask =
                                        s_quad_swing_mask;
                                    s_quad_live_state.search_leg_mask =
                                        (uint8_t)(s_quad_swing_mask &
                                                  ~s_quad_press_mask);
                                    s_quad_demo_plan.active_leg_mask =
                                        s_quad_swing_mask;
                                    rebase_quad_support_locked(&pose,
                                                               landed_mask);
                                }
                                if (s_quad_swing_mask == 0u) {
                                    s_quad_demo_active = false;
                                    s_quad_demo_holding = true;
                                    completed = true;
                                }
                            } else if (pose.complete) {
                                s_quad_demo_active = false;
                                s_quad_demo_holding = true;
                                completed = true;
                            }
                        }
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (failed) {
            ESP_LOGE(TAG,
                     "quad demo stopped at %u ms: invalid pose or I2C failure; /OE HIGH",
                     elapsed_ms);
        } else if (completed) {
            ESP_LOGW(TAG,
                     "quad demo complete; soft_limit_legs=0x%02x; final four-leg target held; /OE LOW; press 1..4 for next step or send stop",
                     s_quad_soft_limit_leg_mask);
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_QUAD_DEMO_UPDATE_MS));
    }
}

static bool start_vertical_sweep(uint16_t positive_hip_us,
                                 uint16_t negative_hip_us)
{
    if (positive_hip_us < SERVO_ZERO_TEST_MIN_US ||
        positive_hip_us > SERVO_ZERO_TEST_MAX_US ||
        negative_hip_us < SERVO_ZERO_TEST_MIN_US ||
        negative_hip_us > SERVO_ZERO_TEST_MAX_US ||
        positive_hip_us <= negative_hip_us || s_outputs_enabled ||
        s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active ||
        s_trace_e2e_running || s_trace_legacy_running || s_trace_local_dry_run ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active ||
        s_trace_e2e_running || s_trace_legacy_running || s_trace_local_dry_run) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    const servo_zero_leg_point_t nominal_rest_foot = {
        .x_mm = 0.0f,
        .z_mm = -100.0f,
    };
    servo_zero_leg_angles_t rest_angles;
    if (!servo_zero_leg_ik(nominal_rest_foot,
                           SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD, &rest_angles)) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    const servo_zero_leg_pulse_map_t pulse_map = {
        .hip_rest_us = servo_zero_rest_target_us(0u),
        .knee_rest_us = servo_zero_rest_target_us(1u),
        .hip_us_per_rad = SERVO_ZERO_RF_MATH_HIP_US_PER_RAD,
        .knee_us_per_rad = SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD,
        .min_us = SERVO_ZERO_TEST_MIN_US,
        .max_us = SERVO_ZERO_TEST_MAX_US,
    };
    const float start_hip_rad =
        rest_angles.hip_rad +
        ((float)positive_hip_us - (float)pulse_map.hip_rest_us) /
            pulse_map.hip_us_per_rad;
    const float end_hip_rad =
        rest_angles.hip_rad +
        ((float)negative_hip_us - (float)pulse_map.hip_rest_us) /
            pulse_map.hip_us_per_rad;
    const servo_zero_leg_vertical_sweep_params_t params = {
        .start_hip_rad = start_hip_rad,
        .end_hip_rad = end_hip_rad,
        .shank_rad = SERVO_ZERO_VERTICAL_SHANK_RAD,
        .move_ms = SERVO_ZERO_VERTICAL_SWEEP_MS,
    };
    servo_zero_leg_vertical_sweep_plan_t plan;
    if (start_hip_rad <= end_hip_rad ||
        !servo_zero_leg_make_vertical_sweep_plan(&params, &plan)) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    const servo_zero_leg_pulse_pose_t start_pulses =
        servo_zero_leg_map_angles_to_pulses(rest_angles, plan.start_angles,
                                            &pulse_map);
    const servo_zero_leg_pulse_pose_t end_pulses =
        servo_zero_leg_map_angles_to_pulses(rest_angles, plan.end_angles,
                                            &pulse_map);
    if (!start_pulses.valid || !end_pulses.valid ||
        start_pulses.hip_us != positive_hip_us ||
        end_pulses.hip_us != negative_hip_us) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGW(TAG,
                 "vertical sweep rejected: CH1 compensation or endpoint mapping is outside %d..%d us",
                 SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
        return false;
    }

    s_vertical_sweep_initial_us[0] = s_pulse_us[0];
    s_vertical_sweep_initial_us[1] = s_pulse_us[1];
    s_pulse_us[0] = start_pulses.hip_us;
    s_pulse_us[1] = start_pulses.knee_us;
    /* PCA9685 /OE is global. Explicitly turn every non-target channel off in
     * the same preload frame before releasing /OE, otherwise CH2..CH7 would
     * immediately start their previously latched PWM targets. The non-target
     * servos therefore lose holding torque and must be mechanically supported
     * or disconnected for this isolated bench test. */
    esp_err_t err = write_vertical_sweep_pair_only_frame();
    if (err != ESP_OK) {
        s_pulse_us[0] = s_vertical_sweep_initial_us[0];
        s_pulse_us[1] = s_vertical_sweep_initial_us[1];
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "vertical sweep preload failed; /OE remains HIGH");
        return false;
    }
    s_vertical_sweep_plan = plan;
    s_vertical_sweep_pulse_map = pulse_map;
    s_vertical_sweep_rest_angles = rest_angles;
    s_vertical_sweep_positive_us = positive_hip_us;
    s_vertical_sweep_negative_us = negative_hip_us;
    s_vertical_sweep_started_us = esp_timer_get_time();
    s_vertical_sweep_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static void abort_vertical_sweep(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_vertical_sweep_active) {
        s_pulse_us[0] = s_vertical_sweep_initial_us[0];
        s_pulse_us[1] = s_vertical_sweep_initial_us[1];
    }
    s_vertical_sweep_active = false;
    xSemaphoreGive(s_i2c_mutex);
}

static void vertical_sweep_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        esp_err_t err = ESP_OK;
        bool completed = false;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_vertical_sweep_active) {
                if (!s_outputs_enabled) {
                    s_pulse_us[0] = s_vertical_sweep_initial_us[0];
                    s_pulse_us[1] = s_vertical_sweep_initial_us[1];
                    s_vertical_sweep_active = false;
                } else {
                    const int64_t elapsed_us =
                        esp_timer_get_time() - s_vertical_sweep_started_us;
                    const uint32_t elapsed_ms =
                        elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
                    const servo_zero_leg_vertical_sweep_pose_t pose =
                        servo_zero_leg_vertical_sweep_pose(
                            elapsed_ms, &s_vertical_sweep_plan);
                    const servo_zero_leg_pulse_pose_t pulses =
                        servo_zero_leg_map_angles_to_pulses(
                            s_vertical_sweep_rest_angles, pose.angles,
                            &s_vertical_sweep_pulse_map);
                    if (!pose.valid || !pulses.valid) {
                        s_pulse_us[0] = s_vertical_sweep_initial_us[0];
                        s_pulse_us[1] = s_vertical_sweep_initial_us[1];
                        s_vertical_sweep_active = false;
                        s_outputs_enabled = false;
                        err = ESP_ERR_INVALID_ARG;
                    } else {
                        const bool changed =
                            s_pulse_us[0] != pulses.hip_us ||
                            s_pulse_us[1] != pulses.knee_us;
                        if (changed) {
                            s_pulse_us[0] = pulses.hip_us;
                            s_pulse_us[1] = pulses.knee_us;
                            err = write_vertical_sweep_pair_only_frame();
                        }
                        if (err != ESP_OK) {
                            s_pulse_us[0] = s_vertical_sweep_initial_us[0];
                            s_pulse_us[1] = s_vertical_sweep_initial_us[1];
                            s_vertical_sweep_active = false;
                            s_outputs_enabled = false;
                        } else if (pose.complete) {
                            s_vertical_sweep_active = false;
                            s_outputs_enabled = false;
                            completed = true;
                        }
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG,
                     "vertical sweep stopped: %s (0x%x); /OE HIGH and start pulse retained",
                     esp_err_to_name(err), err);
        } else if (completed) {
            disable_outputs();
            ESP_LOGW(TAG,
                     "vertical sweep complete: CH0 %u->%u us; shank held at -90 deg; /OE HIGH",
                     s_vertical_sweep_positive_us,
                     s_vertical_sweep_negative_us);
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static bool start_rf_sequence(void)
{
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    for (uint8_t ch = 0; ch < SERVO_ZERO_ACTIVE_CHANNELS; ++ch) {
        s_pulse_us[ch] = servo_zero_rest_upright_us(ch);
        s_rf_sequence_rest_us[ch] =
            servo_zero_rest_target_us_at_amplitude(ch, 100u);
    }
    s_rf_sequence_lift_us[0] = s_rf_sequence_rest_us[0];
    s_rf_sequence_lift_us[1] = (uint16_t)lroundf(
        (float)s_rf_sequence_rest_us[1] - 10.9f *
        SERVO_ZERO_RF_SEQUENCE_LIFT_DEG);
    s_rf_sequence_final_us[0] = (uint16_t)lroundf(
        (float)s_rf_sequence_rest_us[0] + 14.6f *
        SERVO_ZERO_RF_SEQUENCE_HIP_DEG);
    s_rf_sequence_final_us[1] = (uint16_t)lroundf(
        (float)s_rf_sequence_lift_us[1] + 10.9f *
        SERVO_ZERO_RF_SEQUENCE_LOWER_DEG);
    for (uint8_t i = 0; i < 2; ++i) {
        if (s_rf_sequence_lift_us[i] < SERVO_ZERO_TEST_MIN_US ||
            s_rf_sequence_lift_us[i] > SERVO_ZERO_TEST_MAX_US ||
            s_rf_sequence_final_us[i] < SERVO_ZERO_TEST_MIN_US ||
            s_rf_sequence_final_us[i] > SERVO_ZERO_TEST_MAX_US) {
            xSemaphoreGive(s_i2c_mutex);
            ESP_LOGE(TAG, "RF sequence rejected: target outside %d..%d us",
                     SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
            return false;
        }
    }
    esp_err_t err = write_active_with_unused_off_frame();
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        return false;
    }
    s_pulse_dirty = false;
    s_rf_sequence_started_us = esp_timer_get_time();
    s_rf_sequence_phase = SERVO_ZERO_RF_SEQUENCE_PHASE_NEUTRAL;
    s_rf_sequence_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    ESP_LOGW(TAG,
             "RF sequence started: neutral %ums, rest wait %ums, lift knee=%u us, final hip/knee=%u/%u us",
             SERVO_ZERO_RF_SEQUENCE_NEUTRAL_MS,
             SERVO_ZERO_RF_SEQUENCE_REST_WAIT_MS,
             s_rf_sequence_lift_us[1], s_rf_sequence_final_us[0],
             s_rf_sequence_final_us[1]);
    return true;
}

static void abort_rf_sequence(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_rf_sequence_active = false;
        xSemaphoreGive(s_i2c_mutex);
    }
}

static void rf_sequence_task(void *arg)
{
    (void)arg;
    const uint32_t rest_at = SERVO_ZERO_RF_SEQUENCE_NEUTRAL_MS;
    const uint32_t lift_at = rest_at + SERVO_ZERO_RF_SEQUENCE_REST_WAIT_MS;
    const uint32_t final_at = lift_at + SERVO_ZERO_RF_SEQUENCE_LIFT_WAIT_MS;
    for (;;) {
        bool complete = false;
        esp_err_t err = ESP_OK;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_rf_sequence_active) {
                uint32_t elapsed = (uint32_t)((esp_timer_get_time() -
                    s_rf_sequence_started_us) / 1000);
                if (elapsed >= final_at &&
                    s_rf_sequence_phase != SERVO_ZERO_RF_SEQUENCE_PHASE_FINAL) {
                    s_pulse_us[0] = s_rf_sequence_final_us[0];
                    s_pulse_us[1] = s_rf_sequence_final_us[1];
                    err = write_active_with_unused_off_frame();
                    s_rf_sequence_phase = SERVO_ZERO_RF_SEQUENCE_PHASE_FINAL;
                    s_rf_sequence_active = false;
                    complete = true;
                } else if (elapsed >= lift_at &&
                           s_rf_sequence_phase != SERVO_ZERO_RF_SEQUENCE_PHASE_LIFT) {
                    s_pulse_us[0] = s_rf_sequence_lift_us[0];
                    s_pulse_us[1] = s_rf_sequence_lift_us[1];
                    err = write_active_with_unused_off_frame();
                    s_rf_sequence_phase = SERVO_ZERO_RF_SEQUENCE_PHASE_LIFT;
                } else if (elapsed >= rest_at &&
                           s_rf_sequence_phase != SERVO_ZERO_RF_SEQUENCE_PHASE_REST) {
                    for (uint8_t ch = 0; ch < SERVO_ZERO_ACTIVE_CHANNELS; ++ch)
                        s_pulse_us[ch] = s_rf_sequence_rest_us[ch];
                    err = write_active_with_unused_off_frame();
                    s_rf_sequence_phase = SERVO_ZERO_RF_SEQUENCE_PHASE_REST;
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            s_rf_sequence_active = false;
            s_outputs_enabled = false;
            disable_outputs();
            ESP_LOGE(TAG, "RF sequence I2C failure; /OE HIGH and test stopped");
        } else if (complete) {
            ESP_LOGW(TAG, "RF sequence complete; final target held; send stop");
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void probe_i2c_bus(void)
{
    uint8_t found = 0;
    for (uint8_t address = 0x03; address <= 0x77; ++address) {
        esp_err_t err = i2c_master_probe(s_bus, address, 20);
        if (err == ESP_OK) {
            ESP_LOGW(TAG, "I2C device ACK at 0x%02x%s", address,
                     address == CONFIG_SERVO_ZERO_PCA9685_ADDRESS
                         ? " (configured PCA9685)"
                         : "");
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "I2C scan found no ACK; check pull-ups, 3.3V, GND, SDA=%d, SCL=%d",
                 CONFIG_SERVO_ZERO_SDA_GPIO, CONFIG_SERVO_ZERO_SCL_GPIO);
    } else {
        ESP_LOGW(TAG, "I2C scan found %u device(s)", found);
    }
}

static esp_err_t write_all_channels_locked(void)
{
    esp_err_t err;
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    err = write_all_channels();
    xSemaphoreGive(s_i2c_mutex);
    return err;
}

static esp_err_t write_all_channels_traced(uint32_t record_id)
{
    servo_zero_trace_set_current(record_id);
    esp_err_t err = write_all_channels();
    servo_zero_trace_set_current(SERVO_ZERO_TRACE_INVALID_ID);
    return err;
}

static esp_err_t set_channel_full_off(uint8_t channel)
{
    uint8_t bytes[5];
    servo_zero_make_full_off_frame(channel, bytes);
    return transmit_pca(bytes, sizeof(bytes), "full-off write");
}

static esp_err_t initialize_pca9685(void)
{
    esp_err_t err;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_SERVO_ZERO_SDA_GPIO,
        .scl_io_num = CONFIG_SERVO_ZERO_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_bus), TAG,
                        "cannot create I2C bus");

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_SERVO_ZERO_PCA9685_ADDRESS,
        .scl_speed_hz = CONFIG_SERVO_ZERO_I2C_SPEED_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &device_config, &s_pca),
                        TAG, "PCA9685 not found");
    probe_i2c_bus();

    /* Channel frames are sent as one register address followed by four data
     * bytes.  PCA9685 MODE1.AI must be set for the address to advance across
     * ON_L/ON_H/OFF_L/OFF_H; without it, only the first addressed register is
     * updated and the generated PWM frame is invalid. */
    err = write_register(PCA9685_REG_MODE1,
                         PCA9685_MODE1_SLEEP | PCA9685_MODE1_AI);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "MODE1 SLEEP write failed: %s (0x%x), SDA=%d SCL=%d addr=0x%02x",
                 esp_err_to_name(err), err, CONFIG_SERVO_ZERO_SDA_GPIO,
                 CONFIG_SERVO_ZERO_SCL_GPIO, CONFIG_SERVO_ZERO_PCA9685_ADDRESS);
        return err;
    }
    err = write_register(PCA9685_REG_MODE2, PCA9685_MODE2_OUTDRV);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MODE2 write failed: %s (0x%x)", esp_err_to_name(err), err);
        return err;
    }
    err = write_register(PCA9685_REG_PRESCALE,
                         servo_zero_prescale(PCA9685_OSCILLATOR_HZ,
                                              CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PRESCALE write failed: %s (0x%x)", esp_err_to_name(err), err);
        return err;
    }

    for (uint8_t channel = SERVO_ZERO_ACTIVE_CHANNELS;
         channel < SERVO_ZERO_CHANNELS; ++channel) {
        err = set_channel_full_off(channel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "CH%u FULL_OFF write failed: %s (0x%x)", channel,
                     esp_err_to_name(err), err);
            return err;
        }
    }
    err = write_all_channels();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "active channel preload failed: %s (0x%x)",
                 esp_err_to_name(err), err);
        return err;
    }
    /* This is the exact wake transaction used by the first known-good CH0
     * firmware. The oscillator is already stopped by the SLEEP write. */
    err = write_register(PCA9685_REG_MODE1,
                         PCA9685_MODE1_RESTART | PCA9685_MODE1_AI);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MODE1 wake write failed: %s (0x%x)",
                 esp_err_to_name(err), err);
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static void servo_refresh_task(void *arg)
{
    (void)arg;
    for (;;) {
        esp_err_t err = ESP_OK;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            const bool legacy_dry_run =
                s_trace_legacy_running && s_trace_e2e_dry_run;
            if ((s_outputs_enabled || legacy_dry_run) && s_pulse_dirty &&
                !s_rf_step_active && !s_rf_math_active &&
                !s_rest_pose_active && !s_knee_swing_active &&
                !s_ch1_cosine_sweep_active &&
                !s_motionleg_active && !s_rf_sequence_active &&
                !s_vertical_sweep_active && !quad_demo_busy()) {
                const uint32_t record_id = legacy_dry_run
                                                ? take_legacy_trace_record()
                                                : SERVO_ZERO_TRACE_INVALID_ID;
                err = write_all_channels_traced(record_id);
                if (err != ESP_OK) {
                    s_outputs_enabled = false;
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG, "channel refresh failed; /OE HIGH and test stopped");
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SERVO_ZERO_REFRESH_MS));
    }
}

static bool take_trace_target(uint32_t *sequence, uint16_t *pulse_us,
                              uint32_t *record_id)
{
    bool available;
    portENTER_CRITICAL(&s_trace_command_lock);
    available = s_trace_target_pending;
    if (available) {
        *sequence = s_trace_target_sequence;
        *pulse_us = s_trace_target_pulse_us;
        *record_id = s_trace_target_record_id;
        s_trace_target_pending = false;
    }
    portEXIT_CRITICAL(&s_trace_command_lock);
    return available;
}

static uint32_t take_legacy_trace_record(void)
{
    uint32_t record_id;
    portENTER_CRITICAL(&s_trace_command_lock);
    record_id = s_trace_legacy_record_id;
    s_trace_legacy_record_id = SERVO_ZERO_TRACE_INVALID_ID;
    portEXIT_CRITICAL(&s_trace_command_lock);
    return record_id;
}

/* Diagnostic E2E path only. It is disabled until an explicit `trace run`
 * command, keeps one latest target (real-time target semantics), and ticks
 * independently of the app_main serial parser. */
static void trace_e2e_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        if (servo_zero_trace_mode() == SERVO_ZERO_TRACE_E2E &&
            s_trace_e2e_running && (s_outputs_enabled || s_trace_e2e_dry_run)) {
            uint32_t sequence;
            uint16_t pulse_us;
            uint32_t record_id;
            if (take_trace_target(&sequence, &pulse_us, &record_id)) {
                (void)sequence;
                esp_err_t err = ESP_OK;
                if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) ==
                    pdTRUE) {
                    s_pulse_us[s_selected_channel] = pulse_us;
                    servo_zero_trace_set_current(record_id);
                    err = write_all_channels();
                    servo_zero_trace_set_current(SERVO_ZERO_TRACE_INVALID_ID);
                    xSemaphoreGive(s_i2c_mutex);
                } else {
                    err = ESP_ERR_TIMEOUT;
                }
                if (err != ESP_OK) {
                    s_trace_e2e_running = false;
                    s_outputs_enabled = false;
                    disable_outputs();
                    ESP_LOGE(TAG,
                             "trace E2E I2C failure: %s (0x%x); /OE HIGH",
                             esp_err_to_name(err), err);
                }
            }
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static bool start_rf_step(uint8_t amplitude_percent)
{
    if (s_outputs_enabled || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    s_rf_step_start_hip_us = SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US;
    s_rf_step_start_knee_us = SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US;
    s_rf_step_amplitude_percent = amplitude_percent;
    s_pulse_us[0] = s_rf_step_start_hip_us;
    s_pulse_us[1] = s_rf_step_start_knee_us;
    /* CH2..CH7 retain the pose that was present when the RF step began. */
    esp_err_t err = write_active_with_unused_off_frame();
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "RF-only preload failed; /OE remains HIGH");
        return false;
    }
    s_rf_step_started_us = esp_timer_get_time();
    s_rf_step_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static void abort_rf_step(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_rf_step_active) {
        s_pulse_us[0] = s_rf_step_start_hip_us;
        s_pulse_us[1] = s_rf_step_start_knee_us;
    }
    s_rf_step_active = false;
    xSemaphoreGive(s_i2c_mutex);
}

static void rf_step_task(void *arg)
{
    (void)arg;
    for (;;) {
        esp_err_t err = ESP_OK;
        bool completed = false;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_rf_step_active) {
                if (!s_outputs_enabled) {
                    s_pulse_us[0] = s_rf_step_start_hip_us;
                    s_pulse_us[1] = s_rf_step_start_knee_us;
                    s_rf_step_active = false;
                } else {
                    const int64_t elapsed_us =
                        esp_timer_get_time() - s_rf_step_started_us;
                    const uint32_t elapsed_ms = elapsed_us <= 0
                                                    ? 0u
                                                    : (uint32_t)(elapsed_us / 1000);
                    const servo_zero_rf_step_pose_t pose =
                        servo_zero_rf_step_pose_at_amplitude(
                            elapsed_ms, s_rf_step_start_hip_us,
                            s_rf_step_start_knee_us,
                            s_rf_step_amplitude_percent);
                    const bool changed =
                        s_pulse_us[0] != pose.hip_us ||
                        s_pulse_us[1] != pose.knee_us;
                    if (changed) {
                        s_pulse_us[0] = pose.hip_us;
                        s_pulse_us[1] = pose.knee_us;
                        err = write_all_channels();
                    }
                    if (err != ESP_OK) {
                        s_pulse_us[0] = s_rf_step_start_hip_us;
                        s_pulse_us[1] = s_rf_step_start_knee_us;
                        s_rf_step_active = false;
                        s_outputs_enabled = false;
                    } else if (pose.complete) {
                        s_rf_step_active = false;
                        s_outputs_enabled = false;
                        completed = true;
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG, "RF step I2C failure; /OE HIGH and test stopped");
        } else if (completed) {
            disable_outputs();
            ESP_LOGW(TAG, "RF step complete; upright restored; /OE HIGH");
        }
        vTaskDelay(pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static void restore_rf_math_rest_locked(void)
{
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
         ++channel) {
        s_pulse_us[channel] = servo_zero_rest_target_us(channel);
    }
}

static bool start_rf_math_step(float swing_degrees)
{
    if (!isfinite(swing_degrees) || swing_degrees < 0.0f ||
        s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    const servo_zero_leg_point_t nominal_rest_foot = {
        .x_mm = 0.0f,
        .z_mm = -100.0f,
    };
    servo_zero_leg_angles_t nominal_rest_angles;
    const servo_zero_leg_flow_params_t params = {
        .rest_angles = {0.0f, 0.0f},
        .lift_clearance_mm = SERVO_ZERO_RF_MATH_LIFT_MM,
        .hip_swing_rad = swing_degrees * SERVO_ZERO_RF_MATH_RAD_PER_DEG,
        .rest_ms = SERVO_ZERO_RF_MATH_REST_MS,
        .move_ms = SERVO_ZERO_RF_MATH_MOVE_MS,
        .knee_branch = SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
    };
    if (!servo_zero_leg_ik(nominal_rest_foot,
                           SERVO_ZERO_LEG_KNEE_BRANCH_FORWARD,
                           &nominal_rest_angles)) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    servo_zero_leg_flow_params_t resolved_params = params;
    resolved_params.rest_angles = nominal_rest_angles;
    servo_zero_leg_flow_plan_t plan;
    if (!servo_zero_leg_make_flow_plan(&resolved_params, &plan)) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    const servo_zero_leg_pulse_map_t pulse_map = {
        .hip_rest_us = servo_zero_rest_target_us(0),
        .knee_rest_us = servo_zero_rest_target_us(1),
        .hip_us_per_rad = SERVO_ZERO_RF_MATH_HIP_US_PER_RAD,
        .knee_us_per_rad = SERVO_ZERO_RF_MATH_KNEE_US_PER_RAD,
        .min_us = SERVO_ZERO_TEST_MIN_US,
        .max_us = SERVO_ZERO_TEST_MAX_US,
    };
    const servo_zero_leg_pulse_pose_t rest_pulses =
        servo_zero_leg_map_angles_to_pulses(plan.rest_angles,
                                            plan.rest_angles, &pulse_map);
    if (!rest_pulses.valid) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    /* Enter the current static rest candidate first. Only CH0/CH1 will be
     * changed by the motion task; the other six active channels hold rest. */
    restore_rf_math_rest_locked();
    s_pulse_us[0] = rest_pulses.hip_us;
    s_pulse_us[1] = rest_pulses.knee_us;
    esp_err_t err = write_all_channels();
    for (uint8_t channel = SERVO_ZERO_ACTIVE_CHANNELS;
         channel < SERVO_ZERO_CHANNELS && err == ESP_OK; ++channel) {
        err = set_channel_full_off(channel);
    }
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "RF IK rest preload failed; /OE remains HIGH");
        return false;
    }
    s_rf_math_plan = plan;
    s_rf_math_pulse_map = pulse_map;
    s_rf_math_started_us = esp_timer_get_time();
    s_rf_math_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static void abort_rf_math_step(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_rf_math_active) {
        restore_rf_math_rest_locked();
    }
    s_rf_math_active = false;
    xSemaphoreGive(s_i2c_mutex);
}

static void rf_math_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        esp_err_t err = ESP_OK;
        bool completed = false;
        bool invalid = false;
        uint32_t trace_tick_id = SERVO_ZERO_TRACE_INVALID_ID;
        servo_zero_leg_point_t touchdown = {0.0f, 0.0f};
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_rf_math_active) {
                if (!s_outputs_enabled) {
                    restore_rf_math_rest_locked();
                    s_rf_math_active = false;
                } else {
                    trace_tick_id = servo_zero_trace_control_tick();
                    const int64_t elapsed_us =
                        esp_timer_get_time() - s_rf_math_started_us;
                    const uint32_t elapsed_ms = elapsed_us <= 0
                                                    ? 0u
                                                    : (uint32_t)(elapsed_us / 1000);
                    const servo_zero_leg_flow_pose_t pose =
                        servo_zero_leg_flow_pose(elapsed_ms, &s_rf_math_plan);
                    const servo_zero_leg_pulse_pose_t pulses =
                        servo_zero_leg_map_angles_to_pulses(
                            s_rf_math_plan.rest_angles, pose.angles,
                            &s_rf_math_pulse_map);
                    if (!pose.valid || !pulses.valid) {
                        restore_rf_math_rest_locked();
                        s_rf_math_active = false;
                        s_outputs_enabled = false;
                        invalid = true;
                    } else {
                        s_pulse_us[0] = pulses.hip_us;
                        s_pulse_us[1] = pulses.knee_us;
                        /* Both joints latch from one auto-increment frame;
                         * CH2..CH7 retain their preloaded rest values. */
                        err = write_all_channels_traced(trace_tick_id);
                        if (err != ESP_OK) {
                            restore_rf_math_rest_locked();
                            s_rf_math_active = false;
                            s_outputs_enabled = false;
                        } else if (pose.complete) {
                            touchdown = pose.foot;
                            s_rf_math_active = false;
                            s_outputs_enabled = false;
                            completed = true;
                        }
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG, "RF IK step I2C failure; /OE HIGH and test stopped");
        } else if (invalid) {
            disable_outputs();
            ESP_LOGE(TAG,
                     "RF IK step invalid pose/pulse; /OE HIGH and rest retained");
        } else if (completed) {
            disable_outputs();
            ESP_LOGW(TAG,
                     "RF IK step complete; touchdown=(%.1f, %.1f) mm; /OE HIGH",
                     touchdown.x_mm, touchdown.z_mm);
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static bool start_rest_pose(uint8_t amplitude_percent, uint8_t channel_mask,
                            bool pseudo_sync, bool split_frames)
{
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }
    s_quad_demo_rest_ready = false;
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
        s_pulse_us[channel] = servo_zero_rest_upright_us(channel);
    }
    s_rest_pose_amplitude_percent = amplitude_percent;
    esp_err_t err = write_all_channels();
    for (uint8_t channel = SERVO_ZERO_ACTIVE_CHANNELS;
         channel < SERVO_ZERO_CHANNELS && err == ESP_OK; ++channel) {
        err = set_channel_full_off(channel);
    }
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "rest-pose preload failed; /OE remains HIGH");
        return false;
    }
    s_rest_pose_started_us = esp_timer_get_time();
    s_rest_pose_reached = false;
    s_rest_pose_channel_mask = channel_mask;
    s_rest_pose_pseudo_sync = pseudo_sync;
    s_rest_pose_split_frames = split_frames;
    s_rest_pose_split_phase = 0u;
    s_rest_pose_active = true;
    s_trace_local_dry_run =
        servo_zero_trace_mode() == SERVO_ZERO_TRACE_LOCAL;
    s_outputs_enabled = !s_trace_local_dry_run;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, s_trace_local_dry_run ? 1 : 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

/* Lock the confirmed rest snapshot without traversing the upright pose. */
static bool start_direct_rest_hold(void)
{
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active || quad_demo_busy() ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    for (uint8_t channel = 0u; channel < SERVO_ZERO_ACTIVE_CHANNELS;
         ++channel) {
        s_pulse_us[channel] = servo_zero_rest_target_us(channel);
    }
    esp_err_t err = write_all_channels();
    for (uint8_t channel = SERVO_ZERO_ACTIVE_CHANNELS;
         channel < SERVO_ZERO_CHANNELS && err == ESP_OK; ++channel) {
        err = set_channel_full_off(channel);
    }
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "direct rest preload failed; /OE remains HIGH");
        return false;
    }
    reset_quad_demo_foot_state();
    s_rest_pose_active = false;
    s_rest_pose_reached = true;
    s_quad_demo_rest_ready = true;
    s_quad_demo_holding = false;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    ESP_LOGW(TAG,
             "direct rest hold reached; skipped upright transition; /OE LOW; demo quad is ready");
    return true;
}

static void abort_rest_pose(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_rest_pose_active || s_rest_pose_reached) {
        for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
             ++channel) {
            s_pulse_us[channel] = servo_zero_rest_upright_us(channel);
        }
    }
    s_rest_pose_active = false;
    s_rest_pose_reached = false;
    s_quad_demo_rest_ready = false;
    s_rest_pose_pseudo_sync = false;
    s_rest_pose_split_frames = false;
    s_rest_pose_split_phase = 0u;
    xSemaphoreGive(s_i2c_mutex);
}

static void rest_pose_task(void *arg)
{
    (void)arg;
    for (;;) {
        esp_err_t err = ESP_OK;
        bool completed = false;
        uint32_t trace_tick_id = SERVO_ZERO_TRACE_INVALID_ID;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_rest_pose_active) {
                if (!s_outputs_enabled && !s_trace_local_dry_run) {
                    for (uint8_t channel = 0;
                         channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                        s_pulse_us[channel] =
                            servo_zero_rest_upright_us(channel);
                    }
                    s_rest_pose_active = false;
                    s_rest_pose_reached = false;
                    s_quad_demo_rest_ready = false;
                } else if (!s_rest_pose_reached) {
                    trace_tick_id = servo_zero_trace_control_tick();
                    const int64_t elapsed_us =
                        esp_timer_get_time() - s_rest_pose_started_us;
                    const uint32_t elapsed_ms = elapsed_us <= 0
                                                    ? 0u
                                                    : (uint32_t)(elapsed_us / 1000);
                    servo_zero_rest_pose_t pose =
                        s_rest_pose_pseudo_sync
                            ? servo_zero_rest_pseudo_pose_at_amplitude(
                                  elapsed_ms,
                                  s_rest_pose_amplitude_percent)
                            : servo_zero_rest_pose_at_amplitude(
                                  elapsed_ms,
                                  s_rest_pose_amplitude_percent);
                    /* V2 normally updates all eight channels at the same
                     * 20 ms tick.  The split diagnostic mode intentionally
                     * alternates two contiguous four-channel frames: it
                     * reduces the I2C burst size and total bus activity while
                     * preserving the PCA9685's 50 Hz PWM output. */
                    for (uint8_t channel = 0;
                         channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                        if ((s_rest_pose_channel_mask & (1u << channel)) == 0u) {
                            pose.pulse_us[channel] =
                                servo_zero_rest_upright_us(channel);
                        }
                    }
                    bool changed = false;
                    for (uint8_t channel = 0;
                         channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                        changed |= s_pulse_us[channel] != pose.pulse_us[channel];
                    }
                    const bool split_frames =
                        s_rest_pose_split_frames && s_rest_pose_pseudo_sync;
                    if (changed) {
                        for (uint8_t channel = 0;
                             channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                            s_pulse_us[channel] = pose.pulse_us[channel];
                        }
                    }
                    /* A split frame must be sent on every active tick, not
                     * only when an integer-rounded target changes. Otherwise
                     * a plateau in one half can prevent the phase from
                     * advancing and leave the other half one tick behind. */
                    if (split_frames) {
                        const uint8_t first_channel =
                            s_rest_pose_split_phase == 0u ? 0u : 4u;
                        servo_zero_trace_set_current(trace_tick_id);
                        err = write_active_half_frame(first_channel);
                        servo_zero_trace_set_current(
                            SERVO_ZERO_TRACE_INVALID_ID);
                        if (err == ESP_OK) {
                            s_rest_pose_split_phase ^= 1u;
                        }
                    } else if (changed) {
                        err = write_all_channels_traced(trace_tick_id);
                    }
                    if (err != ESP_OK) {
                        for (uint8_t channel = 0;
                             channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                            s_pulse_us[channel] =
                                servo_zero_rest_upright_us(channel);
                        }
                        s_rest_pose_active = false;
                        s_rest_pose_reached = false;
                        s_quad_demo_rest_ready = false;
                        s_outputs_enabled = false;
                    } else if (pose.complete) {
                        s_rest_pose_active = false;
                        s_rest_pose_reached = true;
                        s_quad_demo_rest_ready =
                            s_rest_pose_channel_mask == 0xffu &&
                            !s_rest_pose_pseudo_sync &&
                            !s_rest_pose_split_frames;
                        if (s_quad_demo_rest_ready) {
                            reset_quad_demo_foot_state();
                        }
                        completed = true;
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG,
                     "rest pose I2C frame failure: %s (0x%x), elapsed=%u ms; /OE HIGH and test stopped",
                     esp_err_to_name(err), err,
                     (uint32_t)((esp_timer_get_time() - s_rest_pose_started_us) /
                                1000));
        } else if (completed) {
            ESP_LOGW(TAG,
                     "rest pose reached; holding target; /OE LOW; use stop to release");
        }
        vTaskDelay(pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static bool start_knee_swing(uint8_t amplitude_percent)
{
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    const servo_zero_knee_swing_pose_t initial_pose =
        servo_zero_knee_swing_pose_at_amplitude(0u, amplitude_percent);
    if (!initial_pose.valid) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGE(TAG,
                 "knee swing rejected: nominal endpoints exceed %u..%u us",
                 SERVO_ZERO_KNEE_SWING_MIN_US,
                 SERVO_ZERO_KNEE_SWING_MAX_US);
        return false;
    }
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
         ++channel) {
        s_pulse_us[channel] = initial_pose.pulse_us[channel];
    }
    esp_err_t err = write_all_channels();
    for (uint8_t channel = SERVO_ZERO_ACTIVE_CHANNELS;
         channel < SERVO_ZERO_CHANNELS && err == ESP_OK; ++channel) {
        err = set_channel_full_off(channel);
    }
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "knee-swing preload failed; /OE remains HIGH");
        return false;
    }
    s_knee_swing_amplitude_percent = amplitude_percent;
    s_knee_swing_started_us = esp_timer_get_time();
    s_knee_swing_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static void abort_knee_swing(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_knee_swing_active) {
        for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
             ++channel) {
            s_pulse_us[channel] = servo_zero_rest_upright_us(channel);
        }
    }
    s_knee_swing_active = false;
    xSemaphoreGive(s_i2c_mutex);
}

static bool start_ch1_cosine_sweep(void)
{
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active ||
        xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active) {
        xSemaphoreGive(s_i2c_mutex);
        return false;
    }

    const uint16_t initial_us = s_pulse_us[1];
    if (initial_us < SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US ||
        initial_us > SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US) {
        xSemaphoreGive(s_i2c_mutex);
        ESP_LOGW(TAG,
                 "CH1 cosine sweep rejected: current %u us is outside %u..%u us",
                 initial_us, SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US,
                 SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US);
        return false;
    }

    /* Preload every active channel with its current target. Only CH1 is
     * changed by the sweep task; CH0 and CH2..CH7 therefore keep holding. */
    esp_err_t err = write_active_with_unused_off_frame();
    if (err != ESP_OK) {
        xSemaphoreGive(s_i2c_mutex);
        disable_outputs();
        ESP_LOGE(TAG, "CH1 cosine sweep preload failed; /OE remains HIGH");
        return false;
    }
    s_ch1_cosine_sweep_initial_us = initial_us;
    s_ch1_cosine_sweep_started_us = esp_timer_get_time();
    s_ch1_cosine_sweep_active = true;
    s_outputs_enabled = true;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static void abort_ch1_cosine_sweep(void)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }
    if (s_ch1_cosine_sweep_active) {
        s_pulse_us[1] = s_ch1_cosine_sweep_initial_us;
    }
    s_ch1_cosine_sweep_active = false;
    xSemaphoreGive(s_i2c_mutex);
}

static void ch1_cosine_sweep_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        esp_err_t err = ESP_OK;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_ch1_cosine_sweep_active) {
                if (!s_outputs_enabled) {
                    s_pulse_us[1] = s_ch1_cosine_sweep_initial_us;
                    s_ch1_cosine_sweep_active = false;
                } else {
                    const int64_t elapsed_us =
                        esp_timer_get_time() - s_ch1_cosine_sweep_started_us;
                    const uint32_t elapsed_ms =
                        elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
                    const servo_zero_ch1_cosine_sweep_pose_t pose =
                        servo_zero_ch1_cosine_sweep_pose(
                            elapsed_ms, s_ch1_cosine_sweep_initial_us);
                    if (!pose.valid) {
                        s_ch1_cosine_sweep_active = false;
                        s_outputs_enabled = false;
                        err = ESP_ERR_INVALID_ARG;
                    } else if (s_pulse_us[1] != pose.pulse_us) {
                        s_pulse_us[1] = pose.pulse_us;
                        err = write_active_channel_frame(1u);
                        if (err != ESP_OK) {
                            s_pulse_us[1] = s_ch1_cosine_sweep_initial_us;
                            s_ch1_cosine_sweep_active = false;
                            s_outputs_enabled = false;
                        }
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG,
                     "CH1 cosine sweep stopped: %s (0x%x); /OE HIGH and start pulse retained",
                     esp_err_to_name(err), err);
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_CH1_COSINE_SWEEP_UPDATE_MS));
    }
}

static void knee_swing_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        esp_err_t err = ESP_OK;
        uint32_t trace_tick_id = SERVO_ZERO_TRACE_INVALID_ID;
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (s_knee_swing_active) {
                if (!s_outputs_enabled) {
                    for (uint8_t channel = 0;
                         channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                        s_pulse_us[channel] =
                            servo_zero_rest_upright_us(channel);
                    }
                    s_knee_swing_active = false;
                } else {
                    trace_tick_id = servo_zero_trace_control_tick();
                    const int64_t elapsed_us =
                        esp_timer_get_time() - s_knee_swing_started_us;
                    const uint32_t elapsed_ms =
                        elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
                    const servo_zero_knee_swing_pose_t pose =
                        servo_zero_knee_swing_pose_at_amplitude(
                            elapsed_ms, s_knee_swing_amplitude_percent);
                    if (!pose.valid) {
                        s_knee_swing_active = false;
                        s_outputs_enabled = false;
                        err = ESP_ERR_INVALID_ARG;
                    } else {
                        bool changed = false;
                        for (uint8_t channel = 0;
                             channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
                            changed |= s_pulse_us[channel] !=
                                       pose.pulse_us[channel];
                        }
                        if (changed) {
                            for (uint8_t channel = 0;
                                 channel < SERVO_ZERO_ACTIVE_CHANNELS;
                                 ++channel) {
                                s_pulse_us[channel] = pose.pulse_us[channel];
                            }
                            err = write_all_channels_traced(trace_tick_id);
                        }
                        if (err != ESP_OK) {
                            for (uint8_t channel = 0;
                                 channel < SERVO_ZERO_ACTIVE_CHANNELS;
                                 ++channel) {
                                s_pulse_us[channel] =
                                    servo_zero_rest_upright_us(channel);
                            }
                            s_knee_swing_active = false;
                            s_outputs_enabled = false;
                        }
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
        }
        if (err != ESP_OK) {
            disable_outputs();
            ESP_LOGE(TAG,
                     "knee swing stopped: %s (0x%x); /OE HIGH and neutral retained",
                     esp_err_to_name(err), err);
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(SERVO_ZERO_MOTION_PERIOD_MS));
    }
}

static void print_channels(void)
{
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
        ESP_LOGW(TAG, "CH%u %-7s = %u us%s", channel, s_channel_names[channel],
                 s_pulse_us[channel],
                 channel == s_selected_channel ? "  <selected>" : "");
    }
    ESP_LOGW(TAG, "CH8-CH15 are FULL_OFF (unused)");
}

static void send_uart_ack(uint16_t sequence, uint8_t status)
{
    uint8_t frame[SERVO_UART_ACK_FRAME_BYTES];
    const size_t length = servo_uart_encode_ack(sequence, status, frame);
    (void)write(STDOUT_FILENO, frame, length);
}

static bool binary_motion_busy(void)
{
    return s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
           s_knee_swing_active || s_ch1_cosine_sweep_active ||
           s_motionleg_active || s_rf_sequence_active ||
           s_vertical_sweep_active || quad_demo_busy();
}

static void process_binary_event(const servo_uart_event_t *event)
{
    uint8_t status = SERVO_UART_STATUS_OK;
    ESP_LOGW(TAG, "UART binary frame: cmd=%u seq=%u crc=%s recognized=%s",
             event->command, event->sequence,
             event->crc_valid ? "ok" : "bad",
             event->recognized ? "yes" : "no");
    if (!event->crc_valid) {
        status = SERVO_UART_STATUS_BAD_CRC;
    } else if (!event->recognized) {
        status = event->command == SERVO_UART_SET_TARGET
                     ? SERVO_UART_STATUS_BAD_VERSION
                     : SERVO_UART_STATUS_BAD_COMMAND;
    } else if (event->command == SERVO_UART_SET_TARGET) {
        if (event->channel_mask == 0u || binary_motion_busy()) {
            status = SERVO_UART_STATUS_BUSY;
        } else {
            for (uint8_t channel = 0; channel < SERVO_UART_CHANNEL_COUNT;
                 ++channel) {
                if ((event->channel_mask & (1u << channel)) != 0u &&
                    (event->pulse_us[channel] < SERVO_ZERO_TEST_MIN_US ||
                     event->pulse_us[channel] > SERVO_ZERO_TEST_MAX_US)) {
                    status = SERVO_UART_STATUS_BAD_RANGE;
                    break;
                }
            }
            if (status == SERVO_UART_STATUS_OK &&
                xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                for (uint8_t channel = 0;
                     channel < SERVO_UART_CHANNEL_COUNT; ++channel) {
                    if ((event->channel_mask & (1u << channel)) != 0u) {
                        s_pulse_us[channel] = event->pulse_us[channel];
                    }
                }
                s_uart_target_mask |= event->channel_mask;
                s_pulse_dirty = true;
                xSemaphoreGive(s_i2c_mutex);
            } else if (status == SERVO_UART_STATUS_OK) {
                status = SERVO_UART_STATUS_BUSY;
            }
        }
    } else if (event->command == SERVO_UART_RUN) {
        if (s_uart_target_mask != 0xffu || binary_motion_busy()) {
            status = SERVO_UART_STATUS_BUSY;
        } else if (write_all_channels_locked() != ESP_OK) {
            disable_outputs();
            s_outputs_enabled = false;
            status = SERVO_UART_STATUS_IO;
        } else {
            s_outputs_enabled = true;
            gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
        }
    } else if (event->command == SERVO_UART_STOP) {
        char stop_line[] = "stop";
        process_command(stop_line);
    }
    send_uart_ack(event->sequence, status);
}

static bool parse_channel(const char *text, uint8_t *channel)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (end != text && *end == '\0' && value >= 0 &&
        value < SERVO_ZERO_ACTIVE_CHANNELS) {
        *channel = (uint8_t)value;
        return true;
    }
    for (uint8_t index = 0; index < SERVO_ZERO_ACTIVE_CHANNELS; ++index) {
        if (strcmp(text, s_channel_names[index]) == 0) {
            *channel = index;
            return true;
        }
    }
    return false;
}

static bool parse_channel_mask(char *text, uint8_t *mask)
{
    uint8_t parsed_mask = 0u;
    char *token = strtok(text, ",");
    if (token == NULL) {
        return false;
    }
    if (strcmp(token, "all") == 0) {
        if (strtok(NULL, ",") != NULL) {
            return false;
        }
        *mask = 0xffu;
        return true;
    }
    while (token != NULL) {
        uint8_t channel;
        if (!parse_channel(token, &channel)) {
            return false;
        }
        parsed_mask |= (uint8_t)(1u << channel);
        token = strtok(NULL, ",");
    }
    *mask = parsed_mask;
    return parsed_mask != 0u;
}

static bool parse_amplitude_percent(const char *text,
                                    uint8_t *amplitude_percent)
{
    if (text == NULL) {
        *amplitude_percent = SERVO_ZERO_AMPLITUDE_MAX_PERCENT;
        return true;
    }
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' ||
        value < (long)SERVO_ZERO_AMPLITUDE_MIN_PERCENT ||
        value > (long)SERVO_ZERO_AMPLITUDE_MAX_PERCENT) {
        return false;
    }
    *amplitude_percent = (uint8_t)value;
    return true;
}

static bool parse_pulse_frame(char *text,
                              uint16_t pulses[SERVO_ZERO_ACTIVE_CHANNELS])
{
    char *token = strtok(text, ",");
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
         ++channel) {
        char *end;
        long value;
        if (token == NULL) {
            return false;
        }
        value = strtol(token, &end, 10);
        if (end == token || *end != '\0' ||
            value < SERVO_ZERO_TEST_MIN_US ||
            value > SERVO_ZERO_TEST_MAX_US) {
            return false;
        }
        pulses[channel] = (uint16_t)value;
        token = strtok(NULL, ",");
    }
    return token == NULL;
}

static bool set_selected_pulse(uint16_t pulse_us)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    s_pulse_us[s_selected_channel] = pulse_us;
    s_pulse_dirty = true;
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static bool set_all_pulses(uint16_t pulse_us)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
         ++channel) {
        s_pulse_us[channel] = pulse_us;
    }
    s_pulse_dirty = true;
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static bool set_pulse_frame(
    const uint16_t pulses[SERVO_ZERO_ACTIVE_CHANNELS])
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS;
         ++channel) {
        s_pulse_us[channel] = pulses[channel];
    }
    s_pulse_dirty = true;
    xSemaphoreGive(s_i2c_mutex);
    return true;
}

static bool save_channels(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("servo_cal", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return false;
    }
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
        char key[4];
        snprintf(key, sizeof(key), "p%u", channel);
        err = nvs_set_u16(nvs, key, s_pulse_us[channel]);
        if (err != ESP_OK) {
            nvs_close(nvs);
            return false;
        }
    }
    err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}

static void load_channels(void)
{
    nvs_handle_t nvs;
    if (nvs_open("servo_cal", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
        char key[4];
        uint16_t pulse;
        snprintf(key, sizeof(key), "p%u", channel);
        if (nvs_get_u16(nvs, key, &pulse) == ESP_OK &&
            pulse >= SERVO_ZERO_TEST_MIN_US && pulse <= SERVO_ZERO_TEST_MAX_US) {
            s_pulse_us[channel] = pulse;
        }
    }
    nvs_close(nvs);
}

static const char *trace_mode_name(servo_zero_trace_mode_t mode)
{
    switch (mode) {
    case SERVO_ZERO_TRACE_RX_ONLY:
        return "rx";
    case SERVO_ZERO_TRACE_LOCAL:
        return "local";
    case SERVO_ZERO_TRACE_E2E:
        return "e2e";
    default:
        return "off";
    }
}

static bool start_trace(servo_zero_trace_mode_t mode)
{
    if (mode == SERVO_ZERO_TRACE_OFF || s_outputs_enabled ||
        s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active) {
        return false;
    }
    disable_outputs();
    s_trace_e2e_running = false;
    s_trace_e2e_dry_run = false;
    s_trace_legacy_running = false;
    s_trace_legacy_record_id = SERVO_ZERO_TRACE_INVALID_ID;
    portENTER_CRITICAL(&s_trace_command_lock);
    s_trace_target_pending = false;
    s_trace_target_sequence = 0;
    s_trace_target_pulse_us = 0;
    s_trace_target_record_id = SERVO_ZERO_TRACE_INVALID_ID;
    portEXIT_CRITICAL(&s_trace_command_lock);
    servo_zero_trace_begin(mode);
    ESP_LOGW(TAG, "trace started: mode=%s; outputs remain /OE HIGH",
             trace_mode_name(mode));
    return true;
}

static void halt_trace(void)
{
    s_trace_e2e_running = false;
    s_trace_e2e_dry_run = false;
    s_trace_legacy_running = false;
    s_trace_legacy_record_id = SERVO_ZERO_TRACE_INVALID_ID;
    s_trace_local_dry_run = false;
    s_outputs_enabled = false;
    disable_outputs();
    abort_quad_demo();
    servo_zero_trace_halt();
    ESP_LOGW(TAG, "trace halted; /OE HIGH; use trace dump for records");
}

static bool run_trace_e2e(bool dry_run)
{
    if (servo_zero_trace_mode() != SERVO_ZERO_TRACE_E2E ||
        s_outputs_enabled || s_rf_step_active || s_rf_math_active ||
        s_rest_pose_active || s_knee_swing_active ||
        s_ch1_cosine_sweep_active || s_motionleg_active ||
        s_rf_sequence_active || s_vertical_sweep_active ||
        s_i2c_mutex == NULL) {
        return false;
    }
    if (write_all_channels_locked() != ESP_OK) {
        disable_outputs();
        return false;
    }
    s_trace_e2e_running = true;
    s_trace_e2e_dry_run = dry_run;
    s_outputs_enabled = !dry_run;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, dry_run ? 1 : 0);
    ESP_LOGW(TAG,
             "trace E2E running: latest target wins, 20 ms writer, selected CH%u, /OE %s",
             s_selected_channel, dry_run ? "HIGH (dry-run)" : "LOW");
    return true;
}

static bool run_trace_legacy(bool dry_run)
{
    if (servo_zero_trace_mode() != SERVO_ZERO_TRACE_E2E ||
        s_outputs_enabled || s_trace_e2e_running || s_trace_legacy_running ||
        s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
        s_knee_swing_active || s_ch1_cosine_sweep_active ||
        s_motionleg_active || s_rf_sequence_active || s_vertical_sweep_active ||
        s_i2c_mutex == NULL) {
        return false;
    }
    if (write_all_channels_locked() != ESP_OK) {
        disable_outputs();
        return false;
    }
    s_trace_e2e_dry_run = dry_run;
    s_trace_legacy_running = true;
    s_outputs_enabled = !dry_run;
    gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, dry_run ? 1 : 0);
    ESP_LOGW(TAG,
             "trace legacy running: %u ms refresh, latest target wins, selected CH%u, /OE %s",
             CONFIG_SERVO_ZERO_REFRESH_MS, s_selected_channel,
             dry_run ? "HIGH (dry-run)" : "LOW");
    return true;
}

static bool parse_u32(const char *text, uint32_t *value)
{
    if (text == NULL || value == NULL) {
        return false;
    }
    char *end;
    unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_pulse_us(const char *text, uint16_t *pulse_us)
{
    uint32_t parsed;
    if (pulse_us == NULL || !parse_u32(text, &parsed) ||
        parsed < SERVO_ZERO_TEST_MIN_US ||
        parsed > SERVO_ZERO_TEST_MAX_US) {
        return false;
    }
    *pulse_us = (uint16_t)parsed;
    return true;
}

static bool parse_quad_demo_leg(const char *text, uint8_t *leg)
{
    uint32_t parsed;
    if (leg == NULL || !parse_u32(text, &parsed) ||
        parsed < 1u || parsed > SERVO_ZERO_QUAD_DEMO_LEG_COUNT) {
        return false;
    }
    *leg = (uint8_t)(parsed - 1u);
    return true;
}

static bool process_trace_command(const char *command, const char *argument,
                                  const char *argument2, const char *extra)
{
    if (strcmp(command, "trace") == 0) {
        if (argument == NULL) {
            return false;
        }
        if (strcmp(argument, "begin") == 0 && extra == NULL &&
            argument2 != NULL) {
            servo_zero_trace_mode_t mode;
            if (strcmp(argument2, "rx") == 0) {
                mode = SERVO_ZERO_TRACE_RX_ONLY;
            } else if (strcmp(argument2, "local") == 0) {
                mode = SERVO_ZERO_TRACE_LOCAL;
            } else if (strcmp(argument2, "e2e") == 0) {
                mode = SERVO_ZERO_TRACE_E2E;
            } else {
                return false;
            }
            if (!start_trace(mode)) {
                ESP_LOGW(TAG, "trace begin rejected; outputs/motion must be idle");
            }
            return true;
        }
        if (strcmp(argument, "dump") == 0 && argument2 == NULL &&
            extra == NULL) {
            servo_zero_trace_dump();
            return true;
        }
        if (strcmp(argument, "page") == 0 && argument2 != NULL &&
            extra != NULL) {
            uint32_t offset;
            uint32_t requested_count;
            if (!parse_u32(argument2, &offset) ||
                !parse_u32(extra, &requested_count)) {
                ESP_LOGW(TAG, "trace page rejected; use trace page OFFSET COUNT");
            } else {
                servo_zero_trace_dump_page(offset, requested_count);
            }
            return true;
        }
        if (strcmp(argument, "halt") == 0 && argument2 == NULL &&
            extra == NULL) {
            halt_trace();
            return true;
        }
        if (strcmp(argument, "run") == 0 && argument2 == NULL &&
            extra == NULL) {
            if (!run_trace_e2e(false)) {
                ESP_LOGW(TAG, "trace run rejected; begin e2e first and keep all motion idle");
            }
            return true;
        }
        if (strcmp(argument, "run-dry") == 0 && argument2 == NULL &&
            extra == NULL) {
            if (!run_trace_e2e(true)) {
                ESP_LOGW(TAG, "trace run-dry rejected; begin e2e first and keep all motion idle");
            }
            return true;
        }
        if (strcmp(argument, "run-legacy") == 0 && argument2 == NULL &&
            extra == NULL) {
            if (!run_trace_legacy(false)) {
                ESP_LOGW(TAG, "trace run-legacy rejected; begin e2e first and keep all motion idle");
            }
            return true;
        }
        if (strcmp(argument, "run-legacy-dry") == 0 && argument2 == NULL &&
            extra == NULL) {
            if (!run_trace_legacy(true)) {
                ESP_LOGW(TAG, "trace run-legacy-dry rejected; begin e2e first and keep all motion idle");
            }
            return true;
        }
        if (strcmp(argument, "off") == 0 && argument2 == NULL &&
            extra == NULL) {
            halt_trace();
            servo_zero_trace_begin(SERVO_ZERO_TRACE_OFF);
            return true;
        }
        return false;
    }

    if (strcmp(command, "cmd") != 0 && strcmp(command, "CMD") != 0) {
        return false;
    }
    if (argument == NULL ||
        argument2 == NULL || extra != NULL ||
        (servo_zero_trace_mode() != SERVO_ZERO_TRACE_RX_ONLY &&
         servo_zero_trace_mode() != SERVO_ZERO_TRACE_E2E)) {
        return false;
    }
    uint32_t sequence;
    uint32_t pulse;
    if (!parse_u32(argument, &sequence) || !parse_u32(argument2, &pulse) ||
        pulse < SERVO_ZERO_TEST_MIN_US || pulse > SERVO_ZERO_TEST_MAX_US) {
        ESP_LOGW(TAG, "CMD rejected; use CMD SEQ PULSE with pulse %d..%d us",
                 SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
        return true;
    }
    const uint32_t record_id =
        servo_zero_trace_command_rx(sequence, (uint16_t)pulse);
    servo_zero_trace_control_accept(record_id);
    if (servo_zero_trace_mode() == SERVO_ZERO_TRACE_E2E) {
        if (s_trace_legacy_running) {
            if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_pulse_us[s_selected_channel] = (uint16_t)pulse;
                s_pulse_dirty = true;
                portENTER_CRITICAL(&s_trace_command_lock);
                s_trace_legacy_record_id = record_id;
                portEXIT_CRITICAL(&s_trace_command_lock);
                xSemaphoreGive(s_i2c_mutex);
            } else {
                ESP_LOGW(TAG, "legacy CMD rejected; I2C mutex unavailable");
            }
        } else {
            portENTER_CRITICAL(&s_trace_command_lock);
            s_trace_target_sequence = sequence;
            s_trace_target_pulse_us = (uint16_t)pulse;
            s_trace_target_record_id = record_id;
            s_trace_target_pending = true;
            portEXIT_CRITICAL(&s_trace_command_lock);
        }
    }
    return true;
}

static void process_command(char *line)
{
    char *end;
    long requested;
    char *command = strtok(line, " \t");
    char *argument = strtok(NULL, " \t");
    char *argument2 = strtok(NULL, " \t");
    char *extra = strtok(NULL, " \t");
    char *argument3 = strtok(NULL, " \t");

    if (command == NULL) {
        return;
    }
    const bool is_quad_demo_command =
        strcmp(command, "demo") == 0 && argument != NULL &&
        strcmp(argument, "quad") == 0;
    const bool is_quad_press_or_release =
        is_quad_demo_command && argument2 != NULL &&
        (strcmp(argument2, "press") == 0 ||
         strcmp(argument2, "release") == 0);
    if (process_trace_command(command, argument, argument2, extra)) {
        return;
    }
    if ((s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
         s_knee_swing_active || s_ch1_cosine_sweep_active ||
         s_motionleg_active || s_rf_sequence_active ||
         s_vertical_sweep_active || quad_demo_busy()) &&
        strcmp(command, "stop") != 0 &&
        strcmp(command, "list") != 0 && strcmp(command, "help") != 0) {
        if (!(s_motionleg_active && strcmp(command, "release") == 0) &&
            !(is_quad_demo_command &&
              (s_quad_demo_holding || s_quad_press_active)) &&
            !is_quad_press_or_release) {
            ESP_LOGW(TAG, "command rejected while a motion is active; use stop");
            return;
        }
    }
    if (strcmp(command, "help") == 0) {
        ESP_LOGW(TAG, "select N | pulse US | ms X.XXX | angle DEG | center");
        ESP_LOGW(TAG,
                 "all US | frame US0,US1,US2,US3,US4,US5,US6,US7 | list | stats [reset] | save | load | run | stop | rest | release | motionleg rf DEG | step rf-sequence | demo quad N [PROFILE] | demo quad mask MASK [PROFILE] | demo quad press MASK [PROFILE] | demo quad release MASK | demo rf-lift-cosine-drop [DEG] | demo rf-hip-vertical POS_US NEG_US | demo ch1-cosine | demo knee-swing [PCT] | step rf [PCT] | step rf-ik DEG | pose rest [PCT] [CH[,CH...]] | pose pseudo [PCT] | pose pseudo-split [PCT]");
        ESP_LOGW(TAG, "PCT is 0..100; omitted means 100%% target amplitude");
        ESP_LOGW(TAG, "optional CH list on pose rest moves only those channels (diagnostic)");
        ESP_LOGW(TAG, "pose pseudo overlaps two tested diagonal groups; it is an experimental HIGH RISK profile");
        ESP_LOGW(TAG, "trace begin rx|local|e2e | CMD SEQ PULSE | trace run|run-dry|run-legacy|run-legacy-dry | trace halt | trace dump|page | trace off");
        ESP_LOGW(TAG, "demo knee-swing is HIGH RISK: nominal +/-96 deg, 0.76 s per half swing; use stop");
        ESP_LOGW(TAG,
                 "demo ch1-cosine is HIGH RISK: CH1 1000..2000 us, 2.0 s per half swing, 20 ms updates; use stop");
        ESP_LOGW(TAG,
                 "demo rf-lift-cosine-drop [DEG] is HIGH RISK: default 45 deg, max 60 deg; fast CH1 lift, 1.8 s CH0 cosine swing; send release, then stop");
        ESP_LOGW(TAG,
                 "demo rf-hip-vertical is HIGH RISK: CH0 POS_US->NEG_US backward sweep, CH1 compensates to keep shank at -90 deg; send stop");
        ESP_LOGW(TAG,
                 "demo quad N is HIGH RISK: N=1 RF, 2 LF, 3 RR, 4 LR; foot-space lift/swing/timed-search with the other three stance feet retreating; candidate q->us only; send stop");
        ESP_LOGW(TAG,
                 "demo quad profile is step_mm,stance_mm,lift_mm,lift_ms,swing_ms,search_ms,hip_scale_percent; default: 30,9,40,300,360,120,50; the legacy hip_scale field scales live horizontal foot speed; demo quad N/mask is fixed diagnostic; press MASK [PROFILE] is live knee-first hold; release before swing retraces the knee arc, otherwise SEARCH follows the knee-lift arc downward with the deliberate swing offset retained, removing temporary lift x without crossing the LF mirrored hip limit; support retreat conserves total active +x over the explicit support-leg mask, then each support leg applies its own workspace/pulse capacity independently; one support limit does not stop the others; legacy 4/6-field profiles remain supported");
        ESP_LOGW(TAG,
                 "rest enters all-leg rest mode; rest direct locks the confirmed rest snapshot without upright transition; demo quad requires rest before the first 1..4 step");
        return;
    }
    if (strcmp(command, "list") == 0) {
        print_channels();
        return;
    }
    if (strcmp(command, "stats") == 0) {
        if (argument == NULL && argument2 == NULL && extra == NULL) {
            print_i2c_stats();
        } else if (argument != NULL && strcmp(argument, "reset") == 0 &&
                   argument2 == NULL && extra == NULL) {
            reset_i2c_stats();
        } else {
            ESP_LOGW(TAG, "stats usage: stats | stats reset");
        }
        return;
    }
    if (strcmp(command, "select") == 0 && argument != NULL) {
        uint8_t channel;
        if (parse_channel(argument, &channel)) {
            s_selected_channel = channel;
            ESP_LOGW(TAG, "selected CH%u", channel);
        } else {
            ESP_LOGW(TAG, "channel must be 0..7 or a channel name");
        }
        return;
    }
    if (strcmp(command, "save") == 0) {
        ESP_LOGW(TAG, "%s", save_channels() ? "configuration saved" : "save failed");
        return;
    }
    if (strcmp(command, "load") == 0) {
        load_channels();
        s_pulse_dirty = true;
        ESP_LOGW(TAG, "configuration loaded");
        return;
    }
    if (strcmp(command, "run") == 0) {
        if (s_rf_step_active || s_rf_math_active || s_rest_pose_active ||
            s_knee_swing_active || s_ch1_cosine_sweep_active ||
            s_motionleg_active || s_rf_sequence_active ||
            s_vertical_sweep_active || quad_demo_busy()) {
            ESP_LOGW(TAG, "run rejected while a motion is active");
            return;
        }
        if (write_all_channels_locked() != ESP_OK) {
            disable_outputs();
            s_outputs_enabled = false;
            ESP_LOGE(TAG, "run preload failed; /OE remains HIGH");
            return;
        }
        s_outputs_enabled = true;
        gpio_set_level(CONFIG_SERVO_ZERO_OE_GPIO, 0);
        ESP_LOGW(TAG, "run -> /OE LOW");
        return;
    }
    if (strcmp(command, "stop") == 0) {
        disable_outputs();
        s_outputs_enabled = false;
        s_trace_e2e_running = false;
        s_trace_e2e_dry_run = false;
        s_trace_legacy_running = false;
        s_trace_legacy_record_id = SERVO_ZERO_TRACE_INVALID_ID;
        s_trace_local_dry_run = false;
        abort_rf_step();
        abort_rf_math_step();
        abort_rest_pose();
        abort_knee_swing();
        abort_ch1_cosine_sweep();
        abort_motionleg();
        abort_rf_sequence();
        abort_vertical_sweep();
        abort_quad_demo();
        ESP_LOGW(TAG, "UART stop -> /OE HIGH");
        return;
    }
    if (strcmp(command, "rest") == 0 && argument != NULL &&
        strcmp(argument, "direct") == 0 && argument2 == NULL &&
        extra == NULL) {
        if (start_direct_rest_hold()) {
            ESP_LOGW(TAG, "direct rest ready; press 1..4 to start walking");
        } else {
            ESP_LOGW(TAG,
                     "direct rest rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "rest") == 0 && argument == NULL &&
        argument2 == NULL && extra == NULL) {
        if (start_rest_pose(100u, 0xffu, false, false)) {
            ESP_LOGW(TAG,
                     "all-leg rest mode started; wait for rest pose reached before pressing 1..4");
        } else {
            ESP_LOGW(TAG,
                     "rest rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "release") == 0 && argument == NULL) {
        release_motionleg();
        return;
    }
    if (strcmp(command, "step") == 0 && argument != NULL &&
        strcmp(argument, "rf-sequence") == 0 && argument2 == NULL &&
        extra == NULL) {
        if (start_rf_sequence()) {
            ESP_LOGW(TAG, "HIGH RISK RF sequence started: knee lift 45 deg; after 1 s hip +45 deg and knee -90 deg simultaneously");
        } else {
            ESP_LOGW(TAG, "RF sequence rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "motionleg") == 0 && argument != NULL &&
        strcmp(argument, "rf") == 0 && argument2 != NULL && extra == NULL) {
        char *float_end;
        const float swing_degrees = strtof(argument2, &float_end);
        if (float_end == argument2 || *float_end != '\0' ||
            !isfinite(swing_degrees) || swing_degrees < 0.0f) {
            ESP_LOGW(TAG, "motionleg rf rejected; invalid or non-finite DEG");
            return;
        }
        if (start_motionleg(swing_degrees, SERVO_ZERO_MOTIONLEG_SWING_MS)) {
            ESP_LOGW(TAG,
                     "motionleg rf started: CH1 lift=%.1f deg (%d us physical lift), CH0 cosine swing=%.1f deg/%u ms; send release",
                     SERVO_ZERO_MOTIONLEG_LIFT_DEG,
                     SERVO_ZERO_MOTIONLEG_LIFT_OFFSET_US, swing_degrees,
                     SERVO_ZERO_MOTIONLEG_SWING_MS);
        } else {
            ESP_LOGW(TAG, "motionleg rf rejected; outputs must be stopped and idle or plan is unreachable");
        }
        return;
    }
    if (strcmp(command, "demo") == 0 && argument != NULL &&
        strcmp(argument, "quad") == 0) {
        if (argument2 != NULL &&
            (strcmp(argument2, "press") == 0 ||
             strcmp(argument2, "release") == 0)) {
            uint8_t mask;
            uint32_t parsed_mask;
            if (extra == NULL ||
                (strcmp(argument2, "release") == 0 && argument3 != NULL) ||
                !parse_u32(extra, &parsed_mask) || parsed_mask > 0xffu ||
                !parse_quad_mask(extra, &mask)) {
                ESP_LOGW(TAG,
                         "demo quad press/release rejected; use demo quad press MASK or demo quad release MASK, selecting at most 2 legs");
                return;
            }
            const bool accepted = strcmp(argument2, "press") == 0
                                      ? start_quad_press(mask, argument3)
                                      : release_quad_press(mask);
            if (accepted) {
                ESP_LOGW(TAG, "quad %s mask=0x%02x accepted",
                         argument2, mask);
            } else if (strcmp(argument2, "press") == 0 &&
                       (s_quad_demo_active || s_quad_press_active ||
                        s_quad_demo_holding)) {
                ESP_LOGW(TAG,
                         "quad press mask=0x%02x deferred; current motion/window is busy, retry is safe",
                         mask);
            } else if (strcmp(argument2, "release") == 0 &&
                       (s_quad_demo_active || s_quad_demo_holding)) {
                ESP_LOGW(TAG,
                         "quad release mask=0x%02x deferred; current motion is busy, retry is safe",
                         mask);
            } else {
                ESP_LOGW(TAG, "quad %s mask=0x%02x rejected",
                         argument2, mask);
            }
            return;
        }
        if (argument2 != NULL && strcmp(argument2, "mask") == 0) {
            uint32_t mask;
            if (extra == NULL || !parse_u32(extra, &mask) ||
                mask == 0u || mask > 0x0fu) {
                ESP_LOGW(TAG,
                         "demo quad mask rejected; use demo quad mask MASK [PROFILE], MASK selects up to 2 legs");
                return;
            }
            if (start_quad_demo_mask((uint8_t)mask, argument3, false)) {
                ESP_LOGW(TAG,
                         "quad demo mask=0x%02x accepted; selected legs move together (fixed diagnostic path)",
                         (unsigned)mask);
            } else {
                ESP_LOGW(TAG,
                         "demo quad mask rejected; outputs/motion must be idle, candidate path must pass preflight, and current commanded state must be valid");
            }
            return;
        }
        uint8_t active_leg;
        if (argument2 == NULL || argument3 != NULL ||
            !parse_quad_demo_leg(argument2, &active_leg)) {
            ESP_LOGW(TAG,
                     "demo quad rejected; use demo quad 1..4 (1=RF 2=LF 3=RR 4=LR)");
            return;
        }
        if (start_quad_demo(active_leg, extra)) {
            ESP_LOGW(TAG,
                     "quad demo key=%u accepted: active=%s; other legs use independent stance targets and one atomic CH0-CH7 frame",
                     (unsigned)(active_leg + 1u),
                     s_quad_demo_leg_names[active_leg]);
        } else {
            ESP_LOGW(TAG,
                     "demo quad rejected; outputs/motion must be idle, candidate path must pass preflight, and current commanded state must be valid");
        }
        return;
    }
    if (strcmp(command, "demo") == 0 && argument != NULL &&
        strcmp(argument, "rf-lift-cosine-drop") == 0) {
        float swing_degrees = SERVO_ZERO_MOTIONLEG_DEMO_SWING_DEG;
        if (argument2 != NULL) {
            char *float_end;
            swing_degrees = strtof(argument2, &float_end);
            if (float_end == argument2 || *float_end != '\0' ||
                !isfinite(swing_degrees) || swing_degrees <= 0.0f ||
                swing_degrees > SERVO_ZERO_MOTIONLEG_MAX_SWING_DEG) {
                ESP_LOGW(TAG,
                         "demo rf-lift-cosine-drop rejected; DEG must be >0 and <=%.0f",
                         SERVO_ZERO_MOTIONLEG_MAX_SWING_DEG);
                return;
            }
        }
        if (extra != NULL) {
            ESP_LOGW(TAG,
                     "demo rf-lift-cosine-drop rejected; use [DEG] with no extra arguments");
            return;
        }
        if (start_motionleg(swing_degrees,
                            SERVO_ZERO_MOTIONLEG_DEMO_SWING_MS)) {
            ESP_LOGW(TAG,
                     "HIGH RISK RF lift/cosine/drop started: CH1 fast lift=%.1f deg (%d us) in %u ms, hold=%u ms; CH0 cosine swing=%.1f deg over %u ms; send release to cut and drop knee",
                     SERVO_ZERO_MOTIONLEG_LIFT_DEG,
                     SERVO_ZERO_MOTIONLEG_LIFT_OFFSET_US,
                     SERVO_ZERO_MOTIONLEG_LIFT_MS,
                     SERVO_ZERO_MOTIONLEG_WAIT_MS,
                     swing_degrees,
                     SERVO_ZERO_MOTIONLEG_DEMO_SWING_MS);
        } else {
            ESP_LOGW(TAG,
                     "demo rf-lift-cosine-drop rejected; outputs must be stopped and idle or plan is unreachable");
        }
        return;
    }
    if (strcmp(command, "demo") == 0 && argument != NULL &&
        strcmp(argument, "rf-hip-vertical") == 0) {
        char *unexpected = strtok(NULL, " \t");
        uint16_t positive_hip_us;
        uint16_t negative_hip_us;
        if (unexpected != NULL ||
            !parse_pulse_us(argument2, &positive_hip_us) ||
            !parse_pulse_us(extra, &negative_hip_us) ||
            positive_hip_us <= negative_hip_us) {
            ESP_LOGW(TAG,
                     "demo rf-hip-vertical rejected; use POS_US NEG_US in %d..%d with POS_US > NEG_US",
                     SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
            return;
        }
        if (start_vertical_sweep(positive_hip_us, negative_hip_us)) {
            const servo_zero_leg_vertical_sweep_pose_t start_pose =
                servo_zero_leg_vertical_sweep_pose(
                    0u, &s_vertical_sweep_plan);
            const servo_zero_leg_vertical_sweep_pose_t end_pose =
                servo_zero_leg_vertical_sweep_pose(
                    s_vertical_sweep_plan.move_ms, &s_vertical_sweep_plan);
            const servo_zero_leg_pulse_pose_t start_pulses =
                servo_zero_leg_map_angles_to_pulses(
                    s_vertical_sweep_rest_angles, start_pose.angles,
                    &s_vertical_sweep_pulse_map);
            const servo_zero_leg_pulse_pose_t end_pulses =
                servo_zero_leg_map_angles_to_pulses(
                    s_vertical_sweep_rest_angles, end_pose.angles,
                    &s_vertical_sweep_pulse_map);
            ESP_LOGW(TAG,
                     "HIGH RISK RF vertical-shank sweep started: CH0 %u->%u us backward, CH1 %u->%u us compensation, shank=-90.0 deg, %u ms; CH0/CH1 PWM only, CH2-CH15 FULL_OFF; send stop",
                     start_pulses.hip_us, end_pulses.hip_us,
                     start_pulses.knee_us, end_pulses.knee_us,
                     s_vertical_sweep_plan.move_ms);
        } else {
            ESP_LOGW(TAG,
                     "demo rf-hip-vertical rejected; CH1 compensation or model/pulse range is invalid");
        }
        return;
    }
    if (strcmp(command, "demo") == 0 && argument != NULL &&
        strcmp(argument, "ch1-cosine") == 0) {
        if (argument2 != NULL || extra != NULL) {
            ESP_LOGW(TAG,
                     "demo ch1-cosine rejected; use demo ch1-cosine without arguments");
            return;
        }
        if (start_ch1_cosine_sweep()) {
            ESP_LOGW(TAG,
                     "HIGH RISK CH1 cosine sweep started: %u..%u us, %u ms per half swing, %u ms internal cadence; send stop",
                     SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US,
                     SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US,
                     SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS,
                     SERVO_ZERO_CH1_COSINE_SWEEP_UPDATE_MS);
        } else {
            ESP_LOGW(TAG,
                     "demo ch1-cosine rejected; outputs must be stopped and current CH1 must be within %u..%u us",
                     SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US,
                     SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US);
        }
        return;
    }
    if (strcmp(command, "demo") == 0 && argument != NULL &&
        strcmp(argument, "knee-swing") == 0) {
        uint8_t amplitude_percent;
        if (extra != NULL ||
            !parse_amplitude_percent(argument2, &amplitude_percent)) {
            ESP_LOGW(TAG,
                     "demo knee-swing rejected; use demo knee-swing [PCT] with PCT 0..100");
            return;
        }
        if (start_knee_swing(amplitude_percent)) {
            const servo_zero_knee_swing_pose_t endpoint_a =
                servo_zero_knee_swing_pose_at_amplitude(
                    SERVO_ZERO_KNEE_SWING_SETTLE_MS +
                        SERVO_ZERO_KNEE_SWING_MOVE_MS,
                    amplitude_percent);
            const servo_zero_knee_swing_pose_t endpoint_b =
                servo_zero_knee_swing_pose_at_amplitude(
                    SERVO_ZERO_KNEE_SWING_SETTLE_MS +
                        2u * SERVO_ZERO_KNEE_SWING_MOVE_MS,
                    amplitude_percent);
            ESP_LOGW(TAG,
                     "HIGH RISK knee swing started: nominal +/-%u deg (%u%% scale), hips fixed",
                     SERVO_ZERO_KNEE_SWING_NOMINAL_HALF_TRAVEL_DEG,
                     amplitude_percent);
            ESP_LOGW(TAG,
                     "knee endpoints A CH1/3/5/7=%u/%u/%u/%u us; B=%u/%u/%u/%u us",
                     endpoint_a.pulse_us[1], endpoint_a.pulse_us[3],
                     endpoint_a.pulse_us[5], endpoint_a.pulse_us[7],
                     endpoint_b.pulse_us[1], endpoint_b.pulse_us[3],
                     endpoint_b.pulse_us[5], endpoint_b.pulse_us[7]);
            ESP_LOGW(TAG,
                     "schedule: neutral settle=%u ms, move to endpoint=%u ms, then %u ms per half swing; ESP32 20 ms absolute cadence; send stop immediately on any fault",
                     SERVO_ZERO_KNEE_SWING_SETTLE_MS,
                     SERVO_ZERO_KNEE_SWING_MOVE_MS,
                     SERVO_ZERO_KNEE_SWING_MOVE_MS);
        } else {
            ESP_LOGW(TAG,
                     "demo knee-swing rejected; outputs must be stopped and idle and endpoints must fit software pulse range");
        }
        return;
    }
    if (strcmp(command, "step") == 0 && argument != NULL &&
        strcmp(argument, "rf") == 0) {
        uint8_t amplitude_percent;
        if (extra != NULL || !parse_amplitude_percent(argument2,
                                                      &amplitude_percent)) {
            ESP_LOGW(TAG, "step rf rejected; amplitude must be 0..100%%");
            return;
        }
        if (start_rf_step(amplitude_percent)) {
            const servo_zero_rf_step_pose_t target =
                servo_zero_rf_step_pose_at_amplitude(
                    850u, s_rf_step_start_hip_us, s_rf_step_start_knee_us,
                    amplitude_percent);
            ESP_LOGW(TAG,
                     "RF step started: amplitude=%u%%, CH0 %u->%u us, CH1 %u->%u us, %u ms",
                     amplitude_percent,
                     s_rf_step_start_hip_us,
                     target.hip_us,
                     s_rf_step_start_knee_us,
                     target.knee_us,
                     SERVO_ZERO_RF_STEP_TOTAL_MS);
        } else {
            ESP_LOGW(TAG, "RF step rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "step") == 0 && argument != NULL &&
        strcmp(argument, "rf-ik") == 0) {
        char *float_end;
        const float swing_degrees = argument2 == NULL
                                        ? 0.0f
                                        : strtof(argument2, &float_end);
        if (argument2 == NULL || extra != NULL || float_end == argument2 ||
            *float_end != '\0' || !isfinite(swing_degrees) ||
            swing_degrees < 0.0f) {
            ESP_LOGW(TAG, "step rf-ik rejected; invalid or non-finite DEG and model/pulses must be reachable");
            return;
        }
        if (start_rf_math_step(swing_degrees)) {
            const servo_zero_leg_flow_pose_t touchdown_pose =
                servo_zero_leg_flow_pose(s_rf_math_plan.total_ms,
                                         &s_rf_math_plan);
            const servo_zero_leg_pulse_pose_t touchdown_pulses =
                servo_zero_leg_map_angles_to_pulses(
                    s_rf_math_plan.rest_angles, touchdown_pose.angles,
                    &s_rf_math_pulse_map);
            ESP_LOGW(TAG,
                     "RF IK flow step started: swing=%.1f deg, lift=%.1f mm, rest CH0/1=%u/%u, touchdown CH0/1=%u/%u, %u ms",
                     swing_degrees, SERVO_ZERO_RF_MATH_LIFT_MM,
                     s_rf_math_pulse_map.hip_rest_us,
                     s_rf_math_pulse_map.knee_rest_us,
                     touchdown_pulses.hip_us, touchdown_pulses.knee_us,
                     s_rf_math_plan.total_ms);
        } else {
            ESP_LOGW(TAG,
                     "RF IK step rejected; outputs must be stopped and model/pulses must be reachable");
        }
        return;
    }
    if (strcmp(command, "pose") == 0 && argument != NULL &&
        strcmp(argument, "rest") == 0) {
        uint8_t amplitude_percent;
        uint8_t channel_mask = 0xffu;
        if (!parse_amplitude_percent(argument2, &amplitude_percent)) {
            ESP_LOGW(TAG, "pose rest rejected; amplitude must be 0..100%%");
            return;
        }
        if (extra != NULL) {
            if (!parse_channel_mask(extra, &channel_mask)) {
                ESP_LOGW(TAG,
                         "pose rest rejected; diagnostic CH list must be 0..7, comma-separated, or all");
                return;
            }
        }
        if (start_rest_pose(amplitude_percent, channel_mask, false, false)) {
            ESP_LOGW(TAG,
                     "rest pose started: amplitude=%u%% mask=0x%02x, hips=%u/%u/%u/%u knees=%u/%u/%u/%u, %u ms",
                     amplitude_percent,
                     channel_mask,
                     servo_zero_rest_target_us_at_amplitude(0,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(2,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(4,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(6,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(1,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(3,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(5,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(7,
                                                            amplitude_percent),
                     SERVO_ZERO_REST_TOTAL_MS);
            ESP_LOGW(TAG,
                     "rest pose schedule: settle=%u ms, %u groups x %u ms, masks=0x%02x/0x%02x/0x%02x/0x%02x",
                     SERVO_ZERO_REST_UPRIGHT_SETTLE_MS,
                     SERVO_ZERO_REST_GROUP_COUNT,
                     SERVO_ZERO_REST_GROUP_MOVE_MS,
                     SERVO_ZERO_REST_GROUP_A_MASK,
                     SERVO_ZERO_REST_GROUP_B_MASK,
                     SERVO_ZERO_REST_GROUP_C_MASK,
                     SERVO_ZERO_REST_GROUP_D_MASK);
        } else {
            ESP_LOGW(TAG,
                     "rest pose rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "pose") == 0 && argument != NULL &&
        strcmp(argument, "pseudo") == 0) {
        uint8_t amplitude_percent;
        if (extra != NULL ||
            !parse_amplitude_percent(argument2, &amplitude_percent)) {
            ESP_LOGW(TAG,
                     "pose pseudo rejected; use pose pseudo [PCT] with no extra arguments");
            return;
        }
        if (start_rest_pose(amplitude_percent, 0xffu, true, false)) {
            ESP_LOGW(TAG,
                     "pseudo-sync pose started: amplitude=%u%%, hips=%u/%u/%u/%u knees=%u/%u/%u/%u, %u ms",
                     amplitude_percent,
                     servo_zero_rest_target_us_at_amplitude(0,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(2,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(4,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(6,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(1,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(3,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(5,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(7,
                                                            amplitude_percent),
                     SERVO_ZERO_REST_PSEUDO_TOTAL_MS);
            ESP_LOGW(TAG,
                     "pseudo-sync V2 schedule: settle=%u ms, diagonal masks=0x%02x/0x%02x, phase=%u ms, minimum-jerk move=%u ms, all channels per %u ms frame",
                     SERVO_ZERO_REST_UPRIGHT_SETTLE_MS,
                     SERVO_ZERO_REST_PSEUDO_GROUP_A_MASK,
                     SERVO_ZERO_REST_PSEUDO_GROUP_B_MASK,
                     SERVO_ZERO_REST_PSEUDO_PHASE_MS,
                     SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS,
                     SERVO_ZERO_MOTION_PERIOD_MS);
        } else {
            ESP_LOGW(TAG,
                     "pose pseudo rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "pose") == 0 && argument != NULL &&
        strcmp(argument, "pseudo-split") == 0) {
        uint8_t amplitude_percent;
        if (extra != NULL ||
            !parse_amplitude_percent(argument2, &amplitude_percent)) {
            ESP_LOGW(TAG,
                     "pose pseudo-split rejected; use pose pseudo-split [PCT] with no extra arguments");
            return;
        }
        if (start_rest_pose(amplitude_percent, 0xffu, true, true)) {
            ESP_LOGW(TAG,
                     "pseudo-sync split pose started: amplitude=%u%%, hips=%u/%u/%u/%u knees=%u/%u/%u/%u, %u ms",
                     amplitude_percent,
                     servo_zero_rest_target_us_at_amplitude(0,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(2,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(4,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(6,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(1,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(3,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(5,
                                                            amplitude_percent),
                     servo_zero_rest_target_us_at_amplitude(7,
                                                            amplitude_percent),
                     SERVO_ZERO_REST_PSEUDO_TOTAL_MS);
            ESP_LOGW(TAG,
                     "pseudo-sync split schedule: 20 ms task, alternating CH0-CH3/CH4-CH7 17-byte frames; each channel updates every 40 ms");
        } else {
            ESP_LOGW(TAG,
                     "pose pseudo-split rejected; outputs must be stopped and idle");
        }
        return;
    }
    if (strcmp(command, "center") == 0) {
        if (set_all_pulses(SERVO_ZERO_DEFAULT_US)) {
            ESP_LOGW(TAG, "all channels -> %u us", SERVO_ZERO_DEFAULT_US);
        } else {
            ESP_LOGW(TAG, "center failed: I2C mutex unavailable");
        }
        return;
    }
    if (strcmp(command, "all") == 0 && argument != NULL) {
        requested = strtol(argument, &end, 10);
        if (end == argument || *end != '\0' || requested < SERVO_ZERO_TEST_MIN_US ||
            requested > SERVO_ZERO_TEST_MAX_US) {
            ESP_LOGW(TAG, "all rejected; allowed range is %d..%d us",
                     SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
            return;
        }
        if (set_all_pulses((uint16_t)requested)) {
            ESP_LOGW(TAG, "all channels -> %ld us", requested);
        } else {
            ESP_LOGW(TAG, "all failed: I2C mutex unavailable");
        }
        return;
    }
    if (strcmp(command, "frame") == 0 && argument != NULL &&
        argument2 == NULL && extra == NULL) {
        uint16_t pulses[SERVO_ZERO_ACTIVE_CHANNELS];
        if (!parse_pulse_frame(argument, pulses)) {
            ESP_LOGW(TAG,
                     "frame rejected; use exactly 8 comma-separated values in %d..%d us",
                     SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
            return;
        }
        if (set_pulse_frame(pulses)) {
            ESP_LOGW(TAG,
                     "frame -> %u/%u/%u/%u/%u/%u/%u/%u us",
                     pulses[0], pulses[1], pulses[2], pulses[3], pulses[4],
                     pulses[5], pulses[6], pulses[7]);
        } else {
            ESP_LOGW(TAG, "frame failed: I2C mutex unavailable");
        }
        return;
    }
    if ((strcmp(command, "pulse") == 0 || strcmp(command, "us") == 0) &&
        argument != NULL) {
        requested = strtol(argument, &end, 10);
        if (end == argument || *end != '\0' || requested < SERVO_ZERO_TEST_MIN_US ||
            requested > SERVO_ZERO_TEST_MAX_US) {
            ESP_LOGW(TAG, "pulse rejected; allowed range is %d..%d us",
                     SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
            return;
        }
        set_selected_pulse((uint16_t)requested);
        ESP_LOGW(TAG, "CH%u -> %ld us", s_selected_channel, requested);
        return;
    }
    if (strcmp(command, "ms") == 0 && argument != NULL) {
        float milliseconds = strtof(argument, &end);
        requested = (long)(milliseconds * 1000.0f + 0.5f);
        if (end == argument || *end != '\0' || requested < SERVO_ZERO_TEST_MIN_US ||
            requested > SERVO_ZERO_TEST_MAX_US) {
            ESP_LOGW(TAG, "ms rejected; allowed range is %.3f..%.3f",
                     SERVO_ZERO_TEST_MIN_US / 1000.0f,
                     SERVO_ZERO_TEST_MAX_US / 1000.0f);
            return;
        }
        set_selected_pulse((uint16_t)requested);
        ESP_LOGW(TAG, "CH%u -> %.3f ms (%ld us)", s_selected_channel,
                 milliseconds, requested);
        return;
    }
    if (strcmp(command, "angle") == 0 && argument != NULL) {
        float degrees = strtof(argument, &end);
        requested = (long)(SERVO_ZERO_DEFAULT_US +
                           degrees * SERVO_ZERO_ANGLE_US_PER_DEG + 0.5f);
        if (end == argument || *end != '\0' || requested < SERVO_ZERO_TEST_MIN_US ||
            requested > SERVO_ZERO_TEST_MAX_US) {
            ESP_LOGW(TAG, "angle rejected; use a value mapping to %d..%d us",
                     SERVO_ZERO_TEST_MIN_US, SERVO_ZERO_TEST_MAX_US);
            return;
        }
        set_selected_pulse((uint16_t)requested);
        ESP_LOGW(TAG, "CH%u -> %.2f deg (nominal %ld us)", s_selected_channel,
                 degrees, requested);
        return;
    }
    ESP_LOGW(TAG, "unknown command; type help");
}

void app_main(void)
{
    gpio_config_t oe_config = {
        .pin_bit_mask = 1ULL << CONFIG_SERVO_ZERO_OE_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&oe_config));
    disable_outputs();
    ESP_LOGW(TAG, "/OE HIGH: outputs disabled during initialization");

    for (uint8_t channel = 0; channel < SERVO_ZERO_ACTIVE_CHANNELS; ++channel) {
        s_pulse_us[channel] = CONFIG_SERVO_ZERO_PULSE_US;
    }
    for (uint8_t leg = 0u; leg < SERVO_ZERO_QUAD_DEMO_LEG_COUNT; ++leg) {
        s_quad_demo_foot[leg] = (servo_zero_leg_point_t){
            .x_mm = 0.0f,
            .z_mm = -100.0f,
        };
    }
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);
    load_channels();

    /* Keep the IDF driver's NACK/timeout diagnostics visible while this
     * isolated actuator test is being brought up. */
    esp_log_level_set("i2c.master", ESP_LOG_DEBUG);
    if (initialize_pca9685() != ESP_OK) {
        disable_outputs();
        ESP_LOGE(TAG, "initialization failed; /OE remains HIGH forever");
        return;
    }

    s_i2c_mutex = xSemaphoreCreateMutex();
    if (s_i2c_mutex == NULL) {
        disable_outputs();
        ESP_LOGE(TAG, "cannot create I2C mutex; /OE HIGH and test stopped");
        return;
    }
    /* Keep the active-low OE asserted after boot. GPIO1 is directly wired
     * to OE without an external pull-up, so only explicit UART `run` or a
     * bounded one-shot motion command may enable outputs. */
    s_outputs_enabled = false;
    disable_outputs();
    ESP_LOGW(TAG, "CH0-CH7 READY at %d Hz PWM / %d Hz I2C; /OE HIGH; type run to enable; CH8-CH15 FULL_OFF",
             CONFIG_SERVO_ZERO_PWM_FREQUENCY_HZ, CONFIG_SERVO_ZERO_I2C_SPEED_HZ);
    if (xTaskCreate(servo_refresh_task, "servo_refresh", 2048, NULL, 5,
                   NULL) != pdPASS) {
        disable_outputs();
        ESP_LOGE(TAG, "cannot create refresh task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(rf_step_task, "rf_step", 3072, NULL, 6, NULL) != pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG, "cannot create RF step task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(rf_math_task, "rf_math", 4096, NULL, 6, NULL) != pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG,
                 "cannot create RF IK step task; /OE HIGH and test stopped");
        return;
    }
    /* The rest task builds a full pose while holding the I2C mutex and its
     * error path may traverse the IDF I2C retry/logging stack.  Keep enough
     * headroom for a transient NACK; an overflow here would reboot before
     * the caller can put /OE HIGH. */
    if (xTaskCreate(rest_pose_task, "rest_pose", 4096, NULL, 6, NULL) !=
        pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG, "cannot create rest-pose task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(knee_swing_task, "knee_swing", 3072, NULL, 6, NULL) !=
        pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG,
                 "cannot create knee-swing task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(ch1_cosine_sweep_task, "ch1_cosine", 4096, NULL, 6,
                    NULL) != pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG,
                 "cannot create CH1 cosine-sweep task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(motionleg_task, "motionleg", 4096, NULL, 6, NULL) !=
        pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG, "cannot create motionleg task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(quad_demo_task, "quad_demo", 4096, NULL, 6, NULL) !=
        pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG,
                 "cannot create four-leg demo task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(vertical_sweep_task, "vertical_sweep", 4096, NULL, 6,
                    NULL) != pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG,
                 "cannot create vertical-sweep task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(rf_sequence_task, "rf_sequence", 3072, NULL, 6, NULL) !=
        pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG, "cannot create RF sequence task; /OE HIGH and test stopped");
        return;
    }
    if (xTaskCreate(trace_e2e_task, "trace_e2e", 3072, NULL, 6, NULL) !=
        pdPASS) {
        disable_outputs();
        s_outputs_enabled = false;
        ESP_LOGE(TAG,
                 "cannot create trace E2E task; /OE HIGH and test stopped");
        return;
    }

    servo_zero_tcp_diag_init();

    const int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (stdin_flags < 0 ||
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK) < 0) {
        ESP_LOGW(TAG, "cannot set stdin non-blocking; parser will still yield");
    }
    ESP_LOGW(TAG, "ASCII calibration ready; type help");
    char line[128];
    size_t line_length = 0;
    bool discard_until_newline = false;
    servo_uart_parser_t uart_parser;
    servo_uart_parser_init(&uart_parser);
    int64_t uart_last_byte_us = 0;
    while (true) {
        char tcp_line[SERVO_ZERO_TCP_DIAG_MAX_LINE + 1u];
        if (servo_zero_tcp_diag_take_command(tcp_line, sizeof(tcp_line))) {
            servo_zero_tcp_diag_replyf("TCP_DIAG_COMMAND_BEGIN %s", tcp_line);
            process_command(tcp_line);
            servo_zero_tcp_diag_reply("TCP_DIAG_COMMAND_END");
        }
        char input[32];
        const ssize_t received = read(STDIN_FILENO, input, sizeof(input));
        if (received > 0) {
            for (ssize_t index = 0; index < received; ++index) {
                const uint8_t byte = (uint8_t)input[index];
                const int64_t now_us = esp_timer_get_time();
                if (servo_uart_parser_active(&uart_parser) &&
                    now_us - uart_last_byte_us > 250000) {
                    servo_uart_parser_init(&uart_parser);
                }
                if (servo_uart_parser_active(&uart_parser) ||
                    byte == SERVO_UART_SOF) {
                    servo_uart_event_t event;
                    uart_last_byte_us = now_us;
                    if (servo_uart_parser_feed(&uart_parser, byte, &event)) {
                        process_binary_event(&event);
                    }
                    continue;
                }
                const char character = (char)byte;
                if (character == '\r' || character == '\n') {
                    if (!discard_until_newline && line_length != 0) {
                        line[line_length] = '\0';
                        process_command(line);
                    }
                    line_length = 0;
                    discard_until_newline = false;
                } else if (!discard_until_newline) {
                    if (line_length + 1u < sizeof(line)) {
                        line[line_length++] = character;
                    } else {
                        line_length = 0;
                        discard_until_newline = true;
                        ESP_LOGW(TAG, "command line too long; discarded");
                    }
                }
            }
        } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG, "stdin read failed: errno=%d", errno);
        }
        /* No parser path may wait for the rest of a frame. */
        /* CONFIG_FREERTOS_HZ=100: 1 ms rounds to zero ticks. */
        vTaskDelay(1);
    }
    disable_outputs();
    s_outputs_enabled = false;
}
