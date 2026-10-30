#include "mpu6500_math.h"
#include "qw_check.h"

#include <math.h>

static void test_default_config(void)
{
    qw_mpu6500_config_t config = qw_mpu6500_default_config();
    CHECK_INT_EQ(config.sample_rate_div, 0);
    CHECK_INT_EQ(config.dlpf_config, 3);
    CHECK_INT_EQ(config.accel_range, QW_MPU6500_ACCEL_RANGE_2G);
    CHECK_INT_EQ(config.gyro_range, QW_MPU6500_GYRO_RANGE_250DPS);
}

static void test_default_scale_conversion(void)
{
    qw_mpu6500_config_t config = qw_mpu6500_default_config();
    qw_mpu6500_raw_sample_t raw = {.accel_x = 16384,
                                   .accel_y = -8192,
                                   .accel_z = 0,
                                   .temperature = 3338,
                                   .gyro_x = 131,
                                   .gyro_y = -65,
                                   .gyro_z = 0};
    qw_mpu6500_sample_t sample = {0};
    CHECK_INT_EQ(qw_mpu6500_convert_raw(&raw, &config, &sample), 0);
    CHECK_FLOAT_NEAR(sample.accel_x_g, 1.0, 0.0001);
    CHECK_FLOAT_NEAR(sample.accel_y_g, -0.5, 0.0001);
    CHECK_FLOAT_NEAR(sample.temperature_c, 30.9979, 0.001);
    CHECK_FLOAT_NEAR(sample.gyro_x_dps, 1.0, 0.0001);
    CHECK_FLOAT_NEAR(sample.gyro_y_dps, -65.0 / 131.0, 0.0001);
}

static void test_alternate_ranges_and_invalid_input(void)
{
    qw_mpu6500_config_t config = qw_mpu6500_default_config();
    config.accel_range = QW_MPU6500_ACCEL_RANGE_16G;
    config.gyro_range = QW_MPU6500_GYRO_RANGE_2000DPS;
    qw_mpu6500_raw_sample_t raw = {.accel_x = 2048, .gyro_x = 16};
    qw_mpu6500_sample_t sample = {0};
    CHECK_INT_EQ(qw_mpu6500_convert_raw(&raw, &config, &sample), 0);
    CHECK_FLOAT_NEAR(sample.accel_x_g, 1.0, 0.0001);
    CHECK_FLOAT_NEAR(sample.gyro_x_dps, 16.0 / 16.4, 0.0001);
    config.accel_range = (qw_mpu6500_accel_range_t)4;
    CHECK_INT_EQ(qw_mpu6500_convert_raw(&raw, &config, &sample), -1);
    CHECK_INT_EQ(qw_mpu6500_convert_raw(NULL, &config, &sample), -1);
}

static void test_gravity_compensated_acceleration(void)
{
    qw_mpu6500_sample_t sample = {.accel_x_g = 1.0f,
                                  .accel_y_g = 0.0f,
                                  .accel_z_g = 0.0f};
    CHECK_FLOAT_NEAR(qw_mpu6500_accel_magnitude_g(&sample), 1.0, 0.0001);
    sample.accel_x_g = 0.0f;
    sample.accel_y_g = -1.0f;
    CHECK_FLOAT_NEAR(qw_mpu6500_accel_magnitude_g(&sample), 1.0, 0.0001);

    CHECK_FLOAT_NEAR(
        qw_mpu6500_gravity_compensated_magnitude_g(0.98f, 0.98f), 0.0,
        0.0001);
    CHECK_FLOAT_NEAR(
        qw_mpu6500_gravity_compensated_magnitude_g(1.23f, 0.98f), 0.25,
        0.0001);
    CHECK_FLOAT_NEAR(
        qw_mpu6500_gravity_compensated_magnitude_g(0.73f, 0.98f), 0.25,
        0.0001);
    sample.accel_z_g = NAN;
    CHECK(isnan(qw_mpu6500_accel_magnitude_g(&sample)));
    CHECK(isnan(qw_mpu6500_gravity_compensated_magnitude_g(NAN, 1.0f)));
    CHECK(isnan(qw_mpu6500_gravity_compensated_magnitude_g(1.0f, 0.0f)));
}

static void test_accel_attitude_angles(void)
{
    qw_mpu6500_sample_t sample = {
        .accel_x_g = 0.0f,
        .accel_y_g = 0.0f,
        .accel_z_g = 1.0f,
    };
    CHECK_FLOAT_NEAR(qw_mpu6500_accel_roll_deg(&sample), 0.0, 0.001);
    CHECK_FLOAT_NEAR(qw_mpu6500_accel_pitch_deg(&sample), 0.0, 0.001);
    sample.accel_y_g = 0.8660254f;
    sample.accel_z_g = 0.5f;
    CHECK_FLOAT_NEAR(qw_mpu6500_accel_roll_deg(&sample), 60.0, 0.01);
    sample.accel_y_g = 0.0f;
    sample.accel_z_g = 0.5f;
    sample.accel_x_g = -0.8660254f;
    CHECK_FLOAT_NEAR(qw_mpu6500_accel_pitch_deg(&sample), 60.0, 0.01);
}

int main(void)
{
    RUN_TEST(test_default_config);
    RUN_TEST(test_default_scale_conversion);
    RUN_TEST(test_alternate_ranges_and_invalid_input);
    RUN_TEST(test_gravity_compensated_acceleration);
    RUN_TEST(test_accel_attitude_angles);
    printf("test_mpu6500_math: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
