#ifndef QW_ACTUATOR_PCA9685_H
#define QW_ACTUATOR_PCA9685_H

#include "qw_hal.h"

#include "driver/i2c_master.h"

/* Test/bring-up seam. Passing non-NULL adapters makes the driver independent
 * of ESP-IDF bus construction; passing NULL restores the production adapter
 * factories. The caller owns the adapter contexts and their lifetime. */
void qw_actuator_pca9685_bind_hal(const qw_hal_i2c_device_t *i2c,
                                  const qw_hal_gpio_t *gpio,
                                  const qw_hal_clock_t *clock);
void qw_actuator_pca9685_clear_hal(void);

/* Optional production bus owner. If bound before init, the driver adds its
 * PCA9685 device to this bus and never deletes the shared bus. */
void qw_actuator_pca9685_bind_bus(i2c_master_bus_handle_t bus);

#if defined(QW_PCA9685_HOST_TEST)
/* Host-only seam for exercising the real force_safe_stop implementation with
 * injected GPIO/I2C callbacks. It is not part of the firmware API. */
bool qw_actuator_pca9685_test_force_safe_stop(
    bool initialized,
    const qw_hal_i2c_device_t *i2c,
    const qw_hal_gpio_t *gpio);
#endif

#endif /* QW_ACTUATOR_PCA9685_H */
