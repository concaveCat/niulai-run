#ifndef QW_MPU6500_PROBE_TEST_H
#define QW_MPU6500_PROBE_TEST_H

#include "esp_err.h"

/* Isolated, read-only MPU6500 I2C reachability test. */
esp_err_t qw_mpu6500_probe_test(void);

#endif /* QW_MPU6500_PROBE_TEST_H */
