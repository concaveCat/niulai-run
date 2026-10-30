#include "runtime_imu.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <math.h>
#include <limits.h>
#include "mpu6500.h"
#include "fall_detector.h"

static const char *TAG = "runtime_imu";
static qw_mpu6500_t s_imu;
static qw_mpu6500_config_t s_config;
static bool s_ready;
static TaskHandle_t s_task;
static qw_runtime_imu_snapshot_t s_snapshot;
static portMUX_TYPE s_snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_last_sample_us;
static float s_gravity_baseline_g;
static float s_gravity_sum_g;
static uint8_t s_gravity_sample_count;
static bool s_gravity_calibrated;
static float s_roll_baseline_deg;
static float s_pitch_baseline_deg;
static float s_roll_baseline_sum_deg;
static float s_pitch_baseline_sum_deg;
static float s_roll_filtered_deg;
static float s_pitch_filtered_deg;
static bool s_attitude_initialized;
static int64_t s_attitude_last_us;
static qw_fall_detector_t s_fall_detector;

#define QW_IMU_GRAVITY_CALIBRATION_SAMPLES 25U
#define QW_IMU_GRAVITY_MIN_G 0.85f
#define QW_IMU_GRAVITY_MAX_G 1.15f
#define QW_IMU_STATIC_GYRO_MAX_DPS 5.0f
#define QW_IMU_BASELINE_UPDATE_MAX_DELTA_G 0.05f
#define QW_IMU_BASELINE_EMA_ALPHA 0.01f
#define QW_IMU_ATTITUDE_GYRO_WEIGHT 0.98f

static float normalize_angle_deg(float angle)
{
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

static float blend_angle_deg(float gyro_angle, float accel_angle,
                             float gyro_weight)
{
    const float delta = normalize_angle_deg(accel_angle - gyro_angle);
    return normalize_angle_deg(gyro_angle + (1.0f - gyro_weight) * delta);
}

static bool imu_sample_is_static(float magnitude_g,
                                 const qw_mpu6500_sample_t *sample)
{
    if (sample == NULL || !isfinite(magnitude_g) ||
        magnitude_g < QW_IMU_GRAVITY_MIN_G ||
        magnitude_g > QW_IMU_GRAVITY_MAX_G)
        return false;
    const float gyro = sqrtf(sample->gyro_x_dps * sample->gyro_x_dps +
                              sample->gyro_y_dps * sample->gyro_y_dps +
                              sample->gyro_z_dps * sample->gyro_z_dps);
    return isfinite(gyro) && gyro <= QW_IMU_STATIC_GYRO_MAX_DPS;
}

static void imu_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    unsigned sample_count = 0U;
    for (;;) {
        qw_mpu6500_raw_sample_t raw;
        if (qw_mpu6500_read_raw(&s_imu, &raw) == ESP_OK) {
            ++sample_count;
            qw_mpu6500_sample_t sample;
            if (qw_mpu6500_convert_raw(&raw, &s_config, &sample) == 0) {
                const float measured_g =
                    qw_mpu6500_accel_magnitude_g(&sample);
                const float accel_roll_deg =
                    qw_mpu6500_accel_roll_deg(&sample);
                const float accel_pitch_deg =
                    qw_mpu6500_accel_pitch_deg(&sample);
                const bool clipped =
                    raw.accel_x == INT16_MIN || raw.accel_x == INT16_MAX ||
                    raw.accel_y == INT16_MIN || raw.accel_y == INT16_MAX ||
                    raw.accel_z == INT16_MIN || raw.accel_z == INT16_MAX;
                if (!s_gravity_calibrated) {
                    if (!clipped && imu_sample_is_static(measured_g, &sample)) {
                        s_gravity_sum_g += measured_g;
                        s_roll_baseline_sum_deg += accel_roll_deg;
                        s_pitch_baseline_sum_deg += accel_pitch_deg;
                        ++s_gravity_sample_count;
                        if (s_gravity_sample_count >=
                            QW_IMU_GRAVITY_CALIBRATION_SAMPLES) {
                            s_gravity_baseline_g =
                                s_gravity_sum_g /
                                (float)QW_IMU_GRAVITY_CALIBRATION_SAMPLES;
                            s_roll_baseline_deg =
                                s_roll_baseline_sum_deg /
                                (float)QW_IMU_GRAVITY_CALIBRATION_SAMPLES;
                            s_pitch_baseline_deg =
                                s_pitch_baseline_sum_deg /
                                (float)QW_IMU_GRAVITY_CALIBRATION_SAMPLES;
                            s_gravity_calibrated = true;
                            s_roll_filtered_deg = accel_roll_deg;
                            s_pitch_filtered_deg = accel_pitch_deg;
                            s_attitude_initialized = true;
                        }
                    } else {
                        s_gravity_sum_g = 0.0f;
                        s_roll_baseline_sum_deg = 0.0f;
                        s_pitch_baseline_sum_deg = 0.0f;
                        s_gravity_sample_count = 0U;
                    }
                } else if (imu_sample_is_static(measured_g, &sample) &&
                           fabsf(measured_g - s_gravity_baseline_g) <
                               QW_IMU_BASELINE_UPDATE_MAX_DELTA_G) {
                    s_gravity_baseline_g +=
                        QW_IMU_BASELINE_EMA_ALPHA *
                        (measured_g - s_gravity_baseline_g);
                }
                if (s_gravity_calibrated && s_attitude_initialized) {
                    const int64_t now_us = esp_timer_get_time();
                    float dt_s = s_attitude_last_us > 0
                                     ? (float)(now_us - s_attitude_last_us) /
                                           1000000.0f
                                     : 0.02f;
                    if (!isfinite(dt_s) || dt_s <= 0.0f || dt_s > 0.2f) {
                        dt_s = 0.02f;
                    }
                    s_attitude_last_us = now_us;
                    const float gyro_roll =
                        s_roll_filtered_deg + sample.gyro_x_dps * dt_s;
                    const float gyro_pitch =
                        s_pitch_filtered_deg + sample.gyro_y_dps * dt_s;
                    const bool accel_usable = measured_g >= 0.75f &&
                                              measured_g <= 1.25f;
                    s_roll_filtered_deg = accel_usable
                                              ? blend_angle_deg(
                                                    gyro_roll, accel_roll_deg,
                                                    QW_IMU_ATTITUDE_GYRO_WEIGHT)
                                              : normalize_angle_deg(gyro_roll);
                    s_pitch_filtered_deg = accel_usable
                                               ? blend_angle_deg(
                                                     gyro_pitch,
                                                     accel_pitch_deg,
                                                     QW_IMU_ATTITUDE_GYRO_WEIGHT)
                                               : normalize_angle_deg(gyro_pitch);
                }
                const float roll_deg = s_gravity_calibrated
                                           ? normalize_angle_deg(
                                                 s_roll_filtered_deg -
                                                 s_roll_baseline_deg)
                                           : 0.0f;
                const float pitch_deg = s_gravity_calibrated
                                            ? normalize_angle_deg(
                                                  s_pitch_filtered_deg -
                                                  s_pitch_baseline_deg)
                                            : 0.0f;
                const bool fall_detected = qw_fall_detector_update(
                    &s_fall_detector, roll_deg, pitch_deg,
                    s_gravity_calibrated && isfinite(roll_deg) &&
                        isfinite(pitch_deg));
                const float linear_g =
                    qw_mpu6500_gravity_compensated_magnitude_g(
                        measured_g, s_gravity_baseline_g);
                const qw_runtime_imu_snapshot_t next = {
                    .valid = s_gravity_calibrated && isfinite(linear_g),
                    .clipped = clipped,
                    .accel_magnitude_g = linear_g,
                    .roll_deg = roll_deg,
                    .pitch_deg = pitch_deg,
                    .fall_detected = fall_detected,
                    .age_ms = 0U,
                };
                portENTER_CRITICAL(&s_snapshot_lock);
                s_snapshot = next;
                s_last_sample_us = esp_timer_get_time();
                portEXIT_CRITICAL(&s_snapshot_lock);
            }
            if ((sample_count % 50U) == 0U) {
                if (qw_mpu6500_convert_raw(&raw, &s_config, &sample) == 0) {
                    ESP_LOGD(TAG, "telemetry ax=%.3f ay=%.3f az=%.3f "
                                   "gx=%.2f gy=%.2f gz=%.2f",
                             (double)sample.accel_x_g,
                             (double)sample.accel_y_g,
                             (double)sample.accel_z_g,
                             (double)sample.gyro_x_dps,
                             (double)sample.gyro_y_dps,
                             (double)sample.gyro_z_dps);
                }
            }
        } else {
            ESP_LOGW(TAG, "MPU6500 telemetry read failed");
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(20U));
    }
}

esp_err_t qw_runtime_imu_start(i2c_master_bus_handle_t bus)
{
    if (s_ready || s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = qw_mpu6500_default_config();
    esp_err_t err = qw_mpu6500_init_with_config(
        bus, QW_MPU6500_DEFAULT_ADDRESS, &s_config, &s_imu);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MPU6500 telemetry unavailable: %s",
                 esp_err_to_name(err));
        return err;
    }
    s_ready = true;
    s_gravity_baseline_g = 0.0f;
    s_gravity_sum_g = 0.0f;
    s_gravity_sample_count = 0U;
    s_gravity_calibrated = false;
    s_roll_baseline_deg = 0.0f;
    s_pitch_baseline_deg = 0.0f;
    s_roll_baseline_sum_deg = 0.0f;
    s_pitch_baseline_sum_deg = 0.0f;
    s_roll_filtered_deg = 0.0f;
    s_pitch_filtered_deg = 0.0f;
    s_attitude_initialized = false;
    s_attitude_last_us = 0;
    qw_fall_detector_init(&s_fall_detector);
    if (xTaskCreate(imu_task, "runtime_imu", 3072U, NULL, 4U, &s_task) !=
        pdPASS) {
        s_ready = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "MPU6500 telemetry ready address=0x%02X",
             QW_MPU6500_DEFAULT_ADDRESS);
    return ESP_OK;
}

bool qw_runtime_imu_ready(void)
{
    return s_ready;
}

bool qw_runtime_imu_snapshot(qw_runtime_imu_snapshot_t *out)
{
    if (out == NULL) return false;
    portENTER_CRITICAL(&s_snapshot_lock);
    *out = s_snapshot;
    const int64_t sample_us = s_last_sample_us;
    portEXIT_CRITICAL(&s_snapshot_lock);
    if (!out->valid) return false;
    const int64_t now_us = esp_timer_get_time();
    out->age_ms = sample_us > 0 && now_us >= sample_us
                      ? (uint32_t)((now_us - sample_us) / 1000LL)
                      : UINT32_MAX;
    return true;
}

bool qw_runtime_imu_fall_detected(void)
{
    portENTER_CRITICAL(&s_snapshot_lock);
    const bool detected = s_snapshot.fall_detected;
    portEXIT_CRITICAL(&s_snapshot_lock);
    return detected;
}
