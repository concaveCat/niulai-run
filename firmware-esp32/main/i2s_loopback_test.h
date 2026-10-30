#ifndef QW_I2S_LOOPBACK_TEST_H
#define QW_I2S_LOOPBACK_TEST_H

#include "esp_err.h"

/* Isolated GPIO4 -> GPIO14 I2S electrical loopback test.  This mode never
 * starts BLE, FSM, PCA9685 or an actuator. */
esp_err_t qw_i2s_loopback_test(void);

#endif /* QW_I2S_LOOPBACK_TEST_H */
