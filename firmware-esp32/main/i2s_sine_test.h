#ifndef QW_I2S_SINE_TEST_H
#define QW_I2S_SINE_TEST_H

#include "esp_err.h"

/* Isolated MAX98357A/I2S TX diagnostic. It does not start the microphone,
 * BLE, FSM, PCA9685 or actuator path. */
esp_err_t qw_i2s_sine_test(void);

#endif /* QW_I2S_SINE_TEST_H */
