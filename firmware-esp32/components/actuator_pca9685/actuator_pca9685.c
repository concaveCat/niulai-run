/* PCA9685 16-channel PWM driver for the formal four-leg joint frame.
 *
 * Init order is fixed and safety-critical:
 *   1. /OE asserted high (re-asserted here even though app_main did it at
 *      boot, because /OE high is the motion-disable invariant)
 *   2. I2C master on SDA/SCL at 100 kHz
 *   3. MODE1 SLEEP, MODE2 OUTDRV, PRE_SCALE from the configured PWM frequency
 *   4. all 8 channels written to the calibrated four-leg rest pose
 *   5. MODE1 SLEEP
 *
 * Only the safety gate may wake the chip (via command_frame) and lower /OE
 * (via set_oe, and only ever as the LAST arming step). force_safe_stop always
 * attempts /OE high + neutral values + SLEEP and returns false unless all
 * three operations were confirmed.
 */
#include "sdkconfig.h"

#include "leg_actuator.h"
#include "actuator_pca9685.h"
#include "pca9685_safety.h"
#include "pose_map.h"
#include "qw_hal_espidf.h"
#include "servo_interpolator.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "pca9685";

#define PCA_REG_MODE1 0x00
#define PCA_REG_MODE2 0x01
#define PCA_REG_PRE_SCALE 0xFE
#define PCA_REG_LED0_ON_L 0x06

#define PCA_MODE1_SLEEP 0x10
#define PCA_MODE1_AI 0x20
#define PCA_MODE2_OUTDRV 0x04
#define PCA_COUNTS_MAX 4096u
#define FORMAL_LINK_A_MM 45.0f
#define FORMAL_LINK_B_MM 70.0f
#define FORMAL_HIP_US_PER_RAD 836.5f
#define FORMAL_KNEE_US_PER_RAD 624.5f
#define PCA_ACTION_WRITE_ATTEMPTS 3U

#ifndef CONFIG_NIULAI_FORMAL_FL_HIP_REST_US
#define CONFIG_NIULAI_FORMAL_FL_HIP_REST_US 1672
#endif
#ifndef CONFIG_NIULAI_FORMAL_FL_KNEE_REST_US
#define CONFIG_NIULAI_FORMAL_FL_KNEE_REST_US 1748
#endif
#ifndef CONFIG_NIULAI_FORMAL_FR_HIP_REST_US
#define CONFIG_NIULAI_FORMAL_FR_HIP_REST_US 1494
#endif
#ifndef CONFIG_NIULAI_FORMAL_FR_KNEE_REST_US
#define CONFIG_NIULAI_FORMAL_FR_KNEE_REST_US 1165
#endif
#ifndef CONFIG_NIULAI_FORMAL_RL_HIP_REST_US
#define CONFIG_NIULAI_FORMAL_RL_HIP_REST_US 1764
#endif
#ifndef CONFIG_NIULAI_FORMAL_RL_KNEE_REST_US
#define CONFIG_NIULAI_FORMAL_RL_KNEE_REST_US 1934
#endif
#ifndef CONFIG_NIULAI_FORMAL_RR_HIP_REST_US
#define CONFIG_NIULAI_FORMAL_RR_HIP_REST_US 1368
#endif
#ifndef CONFIG_NIULAI_FORMAL_RR_KNEE_REST_US
#define CONFIG_NIULAI_FORMAL_RR_KNEE_REST_US 1279
#endif
#ifndef CONFIG_NIULAI_FORMAL_MAX_PULSE_STEP_US
#define CONFIG_NIULAI_FORMAL_MAX_PULSE_STEP_US 250
#endif

static i2c_master_dev_handle_t s_dev;
static i2c_master_bus_handle_t s_bus;
static i2c_master_bus_handle_t s_external_bus;
static qw_hal_i2c_device_t s_i2c;
static qw_hal_gpio_t s_gpio;
static qw_hal_clock_t s_clock;
static bool s_initialized;
static bool s_sleeping;
static qw_servo_interpolator_t s_active_interpolators[2];
static uint16_t s_formal_last_pulses[8];
static int64_t s_last_command_us;
static bool s_hal_injected;
static qw_hal_i2c_device_t s_injected_i2c;
static qw_hal_gpio_t s_injected_gpio;
static qw_hal_clock_t s_injected_clock;

static void pca_cleanup(void)
{
    if (!s_hal_injected && s_dev != NULL) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (!s_hal_injected && s_bus != NULL && s_bus != s_external_bus) {
        i2c_del_master_bus(s_bus);
    }
    s_bus = NULL;
    s_i2c = s_hal_injected ? s_injected_i2c : (qw_hal_i2c_device_t){0};
    s_sleeping = false;
    s_initialized = false;
}

static bool pca_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return s_i2c.write != NULL &&
           s_i2c.write(s_i2c.ctx, buf, sizeof(buf),
                       CONFIG_NIULAI_PCA9685_I2C_TIMEOUT_MS) == QW_HAL_OK;
}

static uint16_t pulse_to_counts(uint16_t pulse_us)
{
    const uint64_t counts =
        (uint64_t)pulse_us * PCA_COUNTS_MAX *
        CONFIG_NIULAI_PCA9685_PWM_FREQ_HZ / 1000000u;
    return counts >= PCA_COUNTS_MAX ? PCA_COUNTS_MAX - 1 : (uint16_t)counts;
}

static bool pca_write_channel_pair(uint8_t base_channel, uint16_t pulse_a_us,
                                   uint16_t pulse_b_us)
{
    if (base_channel >= QW_PCA9685_FORMAL_CHANNEL_COUNT - 1U ||
        !qw_pca9685_pulse_within_limit(base_channel, pulse_a_us) ||
        !qw_pca9685_pulse_within_limit(base_channel + 1U, pulse_b_us)) {
        return false;
    }
    const uint16_t off_a = pulse_to_counts(pulse_a_us);
    const uint16_t off_b = pulse_to_counts(pulse_b_us);
    const uint8_t buf[9] = {
        PCA_REG_LED0_ON_L + 4u * base_channel,
        0x00, 0x00, (uint8_t)(off_a & 0xffu), (uint8_t)(off_a >> 8),
        0x00, 0x00, (uint8_t)(off_b & 0xffu), (uint8_t)(off_b >> 8),
    };
    return s_i2c.write != NULL &&
           s_i2c.write(s_i2c.ctx, buf, sizeof(buf),
                       CONFIG_NIULAI_PCA9685_I2C_TIMEOUT_MS) == QW_HAL_OK;
}

static qw_hal_status_t pca_write_full_pulses_once(const uint16_t pulses[8])
{
    if (pulses == NULL || s_i2c.write == NULL) {
        return QW_HAL_INVALID_ARG;
    }
    for (uint8_t channel = 0U;
         channel < QW_PCA9685_FORMAL_CHANNEL_COUNT; channel++) {
        if (!qw_pca9685_pulse_within_limit(channel, pulses[channel])) {
            return QW_HAL_INVALID_ARG;
        }
    }
    uint8_t buf[33] = {PCA_REG_LED0_ON_L};
    for (uint8_t channel = 0; channel < 8U; channel++) {
        const uint16_t off = pulse_to_counts(pulses[channel]);
        const uint8_t offset = (uint8_t)(1U + 4U * channel);
        buf[offset] = 0x00;
        buf[offset + 1U] = 0x00;
        buf[offset + 2U] = (uint8_t)(off & 0xFFU);
        buf[offset + 3U] = (uint8_t)(off >> 8U);
    }
    return s_i2c.write(s_i2c.ctx, buf, sizeof(buf),
                       CONFIG_NIULAI_PCA9685_I2C_TIMEOUT_MS);
}

static bool pca_write_action_frame(const uint16_t pulses[8])
{
    qw_hal_status_t status = QW_HAL_IO_ERROR;
    for (unsigned attempt = 0U; attempt < PCA_ACTION_WRITE_ATTEMPTS;
         attempt++) {
        status = pca_write_full_pulses_once(pulses);
        if (status == QW_HAL_OK) {
            if (attempt > 0U) {
                ESP_LOGE(TAG, "PCA9685 action frame recovered attempt=%u",
                         attempt + 1U);
            }
            return true;
        }
    }
    ESP_LOGE(TAG,
             "PCA9685 action frame write failed status=%s attempts=%u pulses=%u,%u,%u,%u,%u,%u,%u,%u",
             qw_hal_status_name(status), PCA_ACTION_WRITE_ATTEMPTS, pulses[0], pulses[1],
             pulses[2], pulses[3], pulses[4], pulses[5], pulses[6], pulses[7]);
    return false;
}

/* Public FL/FR/RL/RR order maps to diagnostic channel order FR/FL/RR/RL on
 * the carrier. These model-to-servo signs mirror the computer ARD-1 path;
 * they remain replaceable candidates until each joint has mechanical data. */
static const uint16_t s_formal_rest_pulses[8] = {
    CONFIG_NIULAI_FORMAL_FR_HIP_REST_US,
    CONFIG_NIULAI_FORMAL_FR_KNEE_REST_US,
    CONFIG_NIULAI_FORMAL_FL_HIP_REST_US,
    CONFIG_NIULAI_FORMAL_FL_KNEE_REST_US,
    CONFIG_NIULAI_FORMAL_RR_HIP_REST_US,
    CONFIG_NIULAI_FORMAL_RR_KNEE_REST_US,
    CONFIG_NIULAI_FORMAL_RL_HIP_REST_US,
    CONFIG_NIULAI_FORMAL_RL_KNEE_REST_US,
};

static const uint8_t s_formal_channel_for_leg[4] = {2U, 0U, 6U, 4U};
static const float s_formal_hip_sign[4] = {-1.0f, 1.0f, -1.0f, 1.0f};
static const float s_formal_knee_sign[4] = {1.0f, -1.0f, 1.0f, -1.0f};

static bool pca_write_formal_rest(void)
{
    return pca_write_full_pulses_once(s_formal_rest_pulses) == QW_HAL_OK;
}

static bool pca_wake(void)
{
    if (!s_sleeping) {
        return true;
    }
    if (!pca_write_reg(PCA_REG_MODE1, PCA_MODE1_AI)) {
        ESP_LOGE(TAG, "PCA9685 wake failed phase=MODE1_WRITE");
        return false;
    }
    if (s_clock.sleep_us != NULL) {
        s_clock.sleep_us(s_clock.ctx, 500U);
    } else if (s_clock.sleep_ms != NULL) {
        s_clock.sleep_ms(s_clock.ctx, 1U);
    } else {
        ESP_LOGE(TAG, "PCA9685 wake failed phase=NO_CLOCK");
        return false;
    }
    s_sleeping = false;
    return true;
}

static bool formal_rest_angle(uint8_t leg, float *hip_rad, float *knee_rad)
{
    if (leg >= 4U || hip_rad == NULL || knee_rad == NULL) {
        return false;
    }
    const float radius_sq = 10000.0f;
    const float cosine = (radius_sq - FORMAL_LINK_A_MM * FORMAL_LINK_A_MM -
                         FORMAL_LINK_B_MM * FORMAL_LINK_B_MM) /
                        (2.0f * FORMAL_LINK_A_MM * FORMAL_LINK_B_MM);
    /* The computer ARD-1 path uses the forward IK branch for all four
     * public legs. Mirrored servo motion is represented by the pulse-map
     * signs below, not by changing the geometric IK branch for the rear. */
    const float knee = acosf(cosine);
    const float hip = atan2f(-100.0f, 0.0f) -
                      atan2f(FORMAL_LINK_B_MM * sinf(knee),
                             FORMAL_LINK_A_MM +
                                 FORMAL_LINK_B_MM * cosf(knee));
    *hip_rad = hip;
    *knee_rad = knee;
    return isfinite(hip) && isfinite(knee);
}

static bool formal_joint_to_pulse(uint8_t leg,
                                  const qw_motion_joint_target_t *joint,
                                  uint16_t *hip_pulse, uint16_t *knee_pulse)
{
    if (leg >= 4U || joint == NULL || hip_pulse == NULL || knee_pulse == NULL ||
        !joint->valid || !isfinite(joint->hip_rad) ||
        !isfinite(joint->knee_rad)) {
        return false;
    }
    float rest_hip;
    float rest_knee;
    if (!formal_rest_angle(leg, &rest_hip, &rest_knee)) {
        return false;
    }
    const uint8_t base = s_formal_channel_for_leg[leg];
    const float hip = (float)s_formal_rest_pulses[base] +
                      s_formal_hip_sign[leg] * FORMAL_HIP_US_PER_RAD *
                          (joint->hip_rad - rest_hip);
    const float knee = (float)s_formal_rest_pulses[base + 1U] +
                       s_formal_knee_sign[leg] * FORMAL_KNEE_US_PER_RAD *
                           (joint->knee_rad - rest_knee);
    if (!isfinite(hip) || !isfinite(knee) || hip < 0.0f || hip > 4095.0f ||
           knee < 0.0f || knee > 4095.0f) {
        return false;
    }
    *hip_pulse = (uint16_t)lroundf(hip);
    *knee_pulse = (uint16_t)lroundf(knee);
    const bool hip_ok = qw_pca9685_pulse_within_limit(base, *hip_pulse);
    const bool knee_ok =
        qw_pca9685_pulse_within_limit(base + 1U, *knee_pulse);
    if (!hip_ok || !knee_ok) {
        ESP_LOGE(TAG,
                 "formal target out of range leg=%u hip_rad=%.4f knee_rad=%.4f channels=%u/%u pulses=%u/%u hip_ok=%d knee_ok=%d",
                 leg, (double)joint->hip_rad, (double)joint->knee_rad, base,
                 base + 1U, *hip_pulse, *knee_pulse, hip_ok, knee_ok);
    }
    return hip_ok && knee_ok;
}

static uint16_t formal_step_toward(uint16_t previous, uint16_t target)
{
    const int delta = (int)target - (int)previous;
    if (delta > CONFIG_NIULAI_FORMAL_MAX_PULSE_STEP_US) {
        return (uint16_t)(previous + CONFIG_NIULAI_FORMAL_MAX_PULSE_STEP_US);
    }
    if (delta < -CONFIG_NIULAI_FORMAL_MAX_PULSE_STEP_US) {
        return (uint16_t)(previous - CONFIG_NIULAI_FORMAL_MAX_PULSE_STEP_US);
    }
    return target;
}

static bool pca_command_formal_frame(const qw_motion_frame_t *frame)
{
    if (!s_initialized || frame == NULL || frame->command_mask != 0x0FU ||
        frame->joint_target_mask != 0x0FU) {
        ESP_LOGE(TAG,
                 "formal frame rejected init=%d frame=%p mask=0x%02x target_mask=0x%02x",
                 s_initialized, (void *)frame,
                 frame == NULL ? 0U : frame->command_mask,
                 frame == NULL ? 0U : frame->joint_target_mask);
        return false;
    }
    uint16_t target_pulses[8];
    uint16_t pulses[8];
    memcpy(pulses, s_formal_last_pulses, sizeof(pulses));
    for (uint8_t leg = 0; leg < 4U; leg++) {
        uint16_t hip;
        uint16_t knee;
        if (!formal_joint_to_pulse(leg, &frame->joints[leg], &hip, &knee)) {
            ESP_LOGE(TAG, "formal frame rejected leg=%u target_conversion", leg);
            return false;
        }
        const uint8_t base = s_formal_channel_for_leg[leg];
        target_pulses[base] = hip;
        target_pulses[base + 1U] = knee;
    }
    /* ARD-1 supplies an instantaneous joint target for each control tick.
     * Apply the reviewed per-frame bound here so a valid lift is sent as a
     * sequence of safe bounded writes instead of being rejected as a failed
     * actuator transaction and latched by the safety gate. */
    for (uint8_t channel = 0U;
         channel < QW_PCA9685_FORMAL_CHANNEL_COUNT; channel++) {
        pulses[channel] = formal_step_toward(s_formal_last_pulses[channel],
                                             target_pulses[channel]);
    }
    if (!pca_wake()) {
        return false;
    }
    if (!pca_write_action_frame(pulses)) {
        return false;
    }
    memcpy(s_formal_last_pulses, pulses, sizeof(s_formal_last_pulses));
    return true;
}

static bool pca_pose_config_valid(void)
{
    const int32_t hold = CONFIG_NIULAI_PCA9685_PULSE_HOLD_US;
    const int32_t lift_a = CONFIG_NIULAI_PCA9685_PULSE_LIFT_US;
    const int32_t lift_b = CONFIG_NIULAI_PCA9685_PULSE_LIFT_B_US;
    const int32_t amp = CONFIG_NIULAI_PCA9685_PULSE_SWING_AMPLITUDE_US;
    if (!qw_pca9685_limits_valid()) {
        return false;
    }
    for (uint8_t channel = 0U;
         channel < QW_PCA9685_FORMAL_CHANNEL_COUNT; channel++) {
        if (!qw_pca9685_pulse_within_limit(channel,
                                           s_formal_rest_pulses[channel])) {
            return false;
        }
    }
    const uint8_t base = qw_pose_active_leg_base_channel();
    return qw_pca9685_pulse_within_limit(base, hold) &&
           qw_pca9685_pulse_within_limit(base + 1U, hold) &&
           qw_pca9685_pulse_within_limit(base, lift_a) &&
           qw_pca9685_pulse_within_limit(base + 1U, lift_b) &&
           qw_pca9685_pulse_within_limit(base, lift_a - amp) &&
           qw_pca9685_pulse_within_limit(base, lift_a + amp) &&
           qw_pca9685_pulse_within_limit(base + 1U, lift_b - amp) &&
           qw_pca9685_pulse_within_limit(base + 1U, lift_b + amp) &&
           qw_pca9685_pulse_within_limit(
               base, CONFIG_NIULAI_PCA9685_PULSE_SEARCH_A_US) &&
           qw_pca9685_pulse_within_limit(
               base + 1U, CONFIG_NIULAI_PCA9685_PULSE_SEARCH_B_US);
}

static bool pca_command_valid(const leg_command_t *cmd)
{
    if (cmd == NULL || cmd->mode > LEG_CMD_SAFE_STOP ||
        !isfinite(cmd->progress)) {
        return false;
    }
    if ((cmd->mode == LEG_CMD_LIFT || cmd->mode == LEG_CMD_SWING) &&
        (cmd->progress < 0.0f || cmd->progress > 1.0f)) {
        return false;
    }
    return cmd->mode != LEG_CMD_SEARCH || cmd->progress >= 0.0f;
}

static bool pca_init(void)
{
    if (s_initialized) {
        return true;
    }
    if (!pca_pose_config_valid()) {
        ESP_LOGE(TAG, "pose pulse configuration outside safe range");
        return false;
    }

    /* 1. /OE high before anything else touches the chip. */
    s_gpio = s_hal_injected ? s_injected_gpio : qw_hal_espidf_gpio();
    if (s_gpio.configure_output == NULL ||
        s_gpio.configure_output(s_gpio.ctx, CONFIG_NIULAI_PCA9685_OE_PIN,
                                true) != QW_HAL_OK) {
        return false;
    }
    if (s_gpio.write(s_gpio.ctx, CONFIG_NIULAI_PCA9685_OE_PIN, true) !=
        QW_HAL_OK) {
        return false;
    }
    s_clock = s_hal_injected ? s_injected_clock : qw_hal_espidf_clock();

    /* 2. I2C master, 100 kHz, unless a test/bring-up transport was bound. */
    if (s_hal_injected) {
        if (s_i2c.write == NULL || s_clock.now_us == NULL ||
            (s_clock.sleep_us == NULL && s_clock.sleep_ms == NULL)) {
            return false;
        }
    } else {
        if (s_external_bus != NULL) {
            s_bus = s_external_bus;
        } else {
            i2c_master_bus_config_t bus_cfg = {
                .i2c_port = I2C_NUM_0,
                .sda_io_num = CONFIG_NIULAI_PCA9685_SDA_PIN,
                .scl_io_num = CONFIG_NIULAI_PCA9685_SCL_PIN,
                .clk_source = I2C_CLK_SRC_DEFAULT,
                .glitch_ignore_cnt = 7,
                .flags.enable_internal_pullup = 1,
            };
            if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
                ESP_LOGE(TAG, "i2c_new_master_bus failed");
                return false;
            }
        }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_NIULAI_PCA9685_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed");
        pca_cleanup();
        return false;
    }
    s_i2c = qw_hal_espidf_i2c_device(s_dev);
    }

    /* 3. PRE_SCALE is writable only while MODE1.SLEEP is set. */
    if (!pca_write_reg(PCA_REG_MODE1, PCA_MODE1_SLEEP | PCA_MODE1_AI) ||
        !pca_write_reg(PCA_REG_MODE2, PCA_MODE2_OUTDRV) ||
        !pca_write_reg(
            PCA_REG_PRE_SCALE,
            (uint8_t)((25000000u /
                       (4096u * CONFIG_NIULAI_PCA9685_PWM_FREQ_HZ)) -
                      1u))) {
        ESP_LOGE(TAG, "PCA9685 configuration write failed");
        pca_cleanup();
        return false;
    }

    /* 4. All channels at the calibrated four-leg rest pose. /OE is still
     * high, so this preload cannot move a servo. */
    if (!pca_write_formal_rest()) {
        ESP_LOGE(TAG, "PCA9685 formal rest write failed");
        pca_cleanup();
        return false;
    }

    /* 5. SLEEP until the safety gate arms us. */
    if (!pca_write_reg(PCA_REG_MODE1, PCA_MODE1_SLEEP | PCA_MODE1_AI)) {
        ESP_LOGE(TAG, "PCA9685 sleep write failed");
        pca_cleanup();
        return false;
    }
    s_sleeping = true;
    memcpy(s_formal_last_pulses, s_formal_rest_pulses,
           sizeof(s_formal_last_pulses));
    qw_servo_interpolator_reset(&s_active_interpolators[0],
                                CONFIG_NIULAI_PCA9685_PULSE_HOLD_US);
    qw_servo_interpolator_reset(&s_active_interpolators[1],
                                CONFIG_NIULAI_PCA9685_PULSE_HOLD_US);
    s_last_command_us = 0;
    s_initialized = true;
    return true;
}

static bool pca_command_leg(const leg_command_t *cmd)
{
    qw_pose_pulse_t pose;

    if (!s_initialized || !pca_command_valid(cmd)) {
        return false;
    }
    pose = qw_pose_map(cmd);

    if (!qw_pca9685_pulse_within_limit(pose.base_channel, pose.pulse_a_us) ||
        !qw_pca9685_pulse_within_limit(pose.base_channel + 1U,
                                       pose.pulse_b_us)) {
        return false;
    }

    /* Writes only change pulse targets while /OE stays high. Wake is kept in
     * the adapter so the gate sequence remains wake -> hold -> /OE low. */
    if (!pca_wake()) {
        return false;
    }
    const int64_t now_us = s_clock.now_us(s_clock.ctx);
    float dt_s = s_last_command_us == 0
                     ? 1.0f / (float)CONFIG_NIULAI_CONTROL_TICK_HZ
                     : (float)(now_us - s_last_command_us) / 1000000.0f;
    s_last_command_us = now_us;
    /* A delayed control tick must not turn into one large mechanical jump. */
    const float nominal_dt_s =
        1.0f / (float)CONFIG_NIULAI_CONTROL_TICK_HZ;
    if (dt_s <= 0.0f || dt_s > 2.0f * nominal_dt_s) {
        dt_s = nominal_dt_s;
    }
    const qw_servo_limits_t limits = {
        .max_velocity_us_s = CONFIG_NIULAI_SERVO_MAX_VELOCITY_US_S,
        .max_acceleration_us_s2 =
            CONFIG_NIULAI_SERVO_MAX_ACCELERATION_US_S2,
    };
    const uint16_t interpolated_a = qw_servo_interpolator_step(
        &s_active_interpolators[0], pose.pulse_a_us, dt_s, &limits);
    const uint16_t interpolated_b = qw_servo_interpolator_step(
        &s_active_interpolators[1], pose.pulse_b_us, dt_s, &limits);
    return pca_write_channel_pair(pose.base_channel, interpolated_a,
                                  interpolated_b);
}

static bool pca_safe_stop_oe_step(void *ctx)
{
    (void)ctx;
    return s_gpio.write != NULL &&
           s_gpio.write(s_gpio.ctx, CONFIG_NIULAI_PCA9685_OE_PIN, true) ==
               QW_HAL_OK;
}

static bool pca_safe_stop_rest_step(void *ctx)
{
    (void)ctx;
    return pca_write_formal_rest();
}

static bool pca_safe_stop_sleep_step(void *ctx)
{
    (void)ctx;
    return pca_write_reg(PCA_REG_MODE1, PCA_MODE1_SLEEP | PCA_MODE1_AI);
}

static bool pca_force_safe_stop(void)
{
    /* An uninitialized adapter can still have a usable /OE binding after a
     * partial init. Try that independent disable, but never claim rest/SLEEP
     * were confirmed when the I2C device was not initialized. */
    const bool safe_stop_ok = s_initialized
                                  ? qw_pca9685_run_safe_stop_steps(
                                        pca_safe_stop_oe_step,
                                        pca_safe_stop_rest_step,
                                        pca_safe_stop_sleep_step, NULL)
                                  : qw_pca9685_run_safe_stop_steps(
                                        pca_safe_stop_oe_step, NULL, NULL, NULL);
    if (!s_initialized) {
        return false;
    }
    qw_servo_interpolator_reset(&s_active_interpolators[0],
                                CONFIG_NIULAI_PCA9685_PULSE_HOLD_US);
    qw_servo_interpolator_reset(&s_active_interpolators[1],
                                CONFIG_NIULAI_PCA9685_PULSE_HOLD_US);
    s_last_command_us = 0;
    memcpy(s_formal_last_pulses, s_formal_rest_pulses,
           sizeof(s_formal_last_pulses));
    s_sleeping = safe_stop_ok;
    return safe_stop_ok;
}

static bool pca_set_oe(bool level)
{
    if (s_gpio.write == NULL) {
        return false;
    }
    return s_gpio.write(s_gpio.ctx, CONFIG_NIULAI_PCA9685_OE_PIN, level) ==
           QW_HAL_OK;
}

static bool pca_command_frame(const qw_motion_frame_t *frame)
{
    if (frame == NULL || frame->command_mask == 0U) {
        return false;
    }
    if (frame->joint_target_mask != 0U) {
        return pca_command_formal_frame(frame);
    }
    if (frame->command_mask == 0x0FU) {
        bool all_hold = true;
        for (uint8_t leg = 0; leg < 4U; leg++) {
            if (frame->legs[leg].mode != LEG_CMD_HOLD_SUPPORT) {
                all_hold = false;
                break;
            }
        }
        if (all_hold) {
            if (!s_initialized || !pca_wake() || !pca_write_formal_rest()) {
                return false;
            }
            memcpy(s_formal_last_pulses, s_formal_rest_pulses,
                   sizeof(s_formal_last_pulses));
            return true;
        }
    }
    const uint8_t active_index = qw_pose_active_leg_index();
    const uint8_t active_mask = (uint8_t)(1U << active_index);
    if (frame->command_mask != active_mask) {
        return false;
    }
    if (!pca_command_valid(&frame->legs[active_index])) {
        return false;
    }
    return pca_command_leg(&frame->legs[active_index]);
}

const leg_actuator_t qw_actuator_pca9685 = {
    pca_init,
    pca_command_frame,
    pca_force_safe_stop,
    pca_set_oe,
};

void qw_actuator_pca9685_bind_hal(const qw_hal_i2c_device_t *i2c,
                                  const qw_hal_gpio_t *gpio,
                                  const qw_hal_clock_t *clock)
{
    if (i2c == NULL || gpio == NULL || clock == NULL) {
        qw_actuator_pca9685_clear_hal();
        return;
    }
    s_injected_i2c = *i2c;
    s_injected_gpio = *gpio;
    s_injected_clock = *clock;
    s_i2c = s_injected_i2c;
    s_hal_injected = true;
}

void qw_actuator_pca9685_clear_hal(void)
{
    s_hal_injected = false;
    s_injected_i2c = (qw_hal_i2c_device_t){0};
    s_injected_gpio = (qw_hal_gpio_t){0};
    s_injected_clock = (qw_hal_clock_t){0};
}

void qw_actuator_pca9685_bind_bus(i2c_master_bus_handle_t bus)
{
    if (s_initialized) {
        return;
    }
    s_external_bus = bus;
}

#if defined(QW_PCA9685_HOST_TEST)
bool qw_actuator_pca9685_test_force_safe_stop(
    bool initialized,
    const qw_hal_i2c_device_t *i2c,
    const qw_hal_gpio_t *gpio)
{
    s_initialized = initialized;
    s_i2c = i2c == NULL ? (qw_hal_i2c_device_t){0} : *i2c;
    s_gpio = gpio == NULL ? (qw_hal_gpio_t){0} : *gpio;
    return pca_force_safe_stop();
}
#endif
