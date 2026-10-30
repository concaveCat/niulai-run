#ifndef QW_MPU6500_MATH_H
#define QW_MPU6500_MATH_H

#include <stdint.h>

typedef enum {
    QW_MPU6500_ACCEL_RANGE_2G = 0,
    QW_MPU6500_ACCEL_RANGE_4G = 1,
    QW_MPU6500_ACCEL_RANGE_8G = 2,
    QW_MPU6500_ACCEL_RANGE_16G = 3,
} qw_mpu6500_accel_range_t;

typedef enum {
    QW_MPU6500_GYRO_RANGE_250DPS = 0,
    QW_MPU6500_GYRO_RANGE_500DPS = 1,
    QW_MPU6500_GYRO_RANGE_1000DPS = 2,
    QW_MPU6500_GYRO_RANGE_2000DPS = 3,
} qw_mpu6500_gyro_range_t;

typedef struct {
    uint8_t sample_rate_div;
    uint8_t dlpf_config;
    qw_mpu6500_accel_range_t accel_range;
    qw_mpu6500_gyro_range_t gyro_range;
} qw_mpu6500_config_t;

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temperature;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} qw_mpu6500_raw_sample_t;

typedef struct {
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float temperature_c;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
} qw_mpu6500_sample_t;

qw_mpu6500_config_t qw_mpu6500_default_config(void);
int qw_mpu6500_convert_raw(const qw_mpu6500_raw_sample_t *raw, const qw_mpu6500_config_t *config, qw_mpu6500_sample_t *sample);
float qw_mpu6500_accel_magnitude_g(const qw_mpu6500_sample_t *sample);
float qw_mpu6500_accel_roll_deg(const qw_mpu6500_sample_t *sample);
float qw_mpu6500_accel_pitch_deg(const qw_mpu6500_sample_t *sample);
float qw_mpu6500_gravity_compensated_magnitude_g(float measured_g,
                                                  float gravity_baseline_g);

#endif /* QW_MPU6500_MATH_H */
