#include "mpu6500_math.h"

#include <stddef.h>
#include <math.h>

static const float ACCEL_LSB_PER_G[] = {16384.0f, 8192.0f, 4096.0f,
                                        2048.0f};
static const float GYRO_LSB_PER_DPS[] = {131.0f, 65.5f, 32.8f, 16.4f};

qw_mpu6500_config_t qw_mpu6500_default_config(void)
{
    return (qw_mpu6500_config_t){
        .sample_rate_div = 0,
        .dlpf_config = 3,
        .accel_range = QW_MPU6500_ACCEL_RANGE_2G,
        .gyro_range = QW_MPU6500_GYRO_RANGE_250DPS,
    };
}

int qw_mpu6500_convert_raw(const qw_mpu6500_raw_sample_t *raw,
                           const qw_mpu6500_config_t *config,
                           qw_mpu6500_sample_t *sample)
{
    if (raw == NULL || config == NULL || sample == NULL ||
        config->accel_range > QW_MPU6500_ACCEL_RANGE_16G ||
        config->gyro_range > QW_MPU6500_GYRO_RANGE_2000DPS) {
        return -1;
    }

    const float accel_scale = ACCEL_LSB_PER_G[config->accel_range];
    const float gyro_scale = GYRO_LSB_PER_DPS[config->gyro_range];
    sample->accel_x_g = (float)raw->accel_x / accel_scale;
    sample->accel_y_g = (float)raw->accel_y / accel_scale;
    sample->accel_z_g = (float)raw->accel_z / accel_scale;
    sample->temperature_c = (float)raw->temperature / 333.87f + 21.0f;
    sample->gyro_x_dps = (float)raw->gyro_x / gyro_scale;
    sample->gyro_y_dps = (float)raw->gyro_y / gyro_scale;
    sample->gyro_z_dps = (float)raw->gyro_z / gyro_scale;
    return 0;
}

float qw_mpu6500_accel_magnitude_g(const qw_mpu6500_sample_t *sample)
{
    if (sample == NULL || !isfinite(sample->accel_x_g) ||
        !isfinite(sample->accel_y_g) || !isfinite(sample->accel_z_g))
        return NAN;
    return sqrtf(sample->accel_x_g * sample->accel_x_g +
                 sample->accel_y_g * sample->accel_y_g +
                 sample->accel_z_g * sample->accel_z_g);
}

float qw_mpu6500_accel_roll_deg(const qw_mpu6500_sample_t *sample)
{
    if (sample == NULL || !isfinite(sample->accel_x_g) ||
        !isfinite(sample->accel_y_g) || !isfinite(sample->accel_z_g)) {
        return NAN;
    }
    return atan2f(sample->accel_y_g, sample->accel_z_g) *
           (180.0f / 3.14159265358979323846f);
}

float qw_mpu6500_accel_pitch_deg(const qw_mpu6500_sample_t *sample)
{
    if (sample == NULL || !isfinite(sample->accel_x_g) ||
        !isfinite(sample->accel_y_g) || !isfinite(sample->accel_z_g)) {
        return NAN;
    }
    const float horizontal =
        sqrtf(sample->accel_y_g * sample->accel_y_g +
              sample->accel_z_g * sample->accel_z_g);
    return atan2f(-sample->accel_x_g, horizontal) *
           (180.0f / 3.14159265358979323846f);
}

float qw_mpu6500_gravity_compensated_magnitude_g(float measured_g,
                                                  float gravity_baseline_g)
{
    if (!isfinite(measured_g) || !isfinite(gravity_baseline_g) ||
        measured_g < 0.0f || gravity_baseline_g <= 0.0f)
        return NAN;
    return fabsf(measured_g - gravity_baseline_g);
}
