#ifndef QW_MPU6500_H
#define QW_MPU6500_H

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "mpu6500_math.h"
#include "qw_hal.h"

#define QW_MPU6500_DEFAULT_ADDRESS 0x68
#define QW_MPU6500_WHO_AM_I 0x70
#define QW_MPU6500_REG_WHO_AM_I 0x75

typedef struct {
    i2c_master_dev_handle_t device;
    qw_hal_i2c_device_t transport;
} qw_mpu6500_t;

esp_err_t qw_mpu6500_init(i2c_master_bus_handle_t bus, uint8_t address,
                          qw_mpu6500_t *out);
esp_err_t qw_mpu6500_init_with_config(i2c_master_bus_handle_t bus,
                                      uint8_t address,
                                      const qw_mpu6500_config_t *config,
                                      qw_mpu6500_t *out);
esp_err_t qw_mpu6500_init_with_transport(
    const qw_hal_i2c_device_t *transport, const qw_mpu6500_config_t *config,
    qw_mpu6500_t *out);
esp_err_t qw_mpu6500_read_register(const qw_mpu6500_t *imu, uint8_t reg,
                                   uint8_t *value);
esp_err_t qw_mpu6500_read_who_am_i(const qw_mpu6500_t *imu, uint8_t *value);
esp_err_t qw_mpu6500_read_raw(const qw_mpu6500_t *imu,
                              qw_mpu6500_raw_sample_t *sample);
esp_err_t qw_mpu6500_write_register(const qw_mpu6500_t *imu, uint8_t reg,
                                    uint8_t value);

#endif /* QW_MPU6500_H */
