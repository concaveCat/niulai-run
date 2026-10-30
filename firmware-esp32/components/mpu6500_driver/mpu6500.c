#include "mpu6500.h"
#include "qw_hal_espidf.h"

#include <string.h>

#define MPU_REG_SMPLRT_DIV 0x19
#define MPU_REG_CONFIG 0x1A
#define MPU_REG_GYRO_CONFIG 0x1B
#define MPU_REG_ACCEL_CONFIG 0x1C
#define MPU_REG_ACCEL_CONFIG2 0x1D
#define MPU_REG_ACCEL_XOUT_H 0x3B
#define MPU_REG_PWR_MGMT_1 0x6B
#define MPU_REG_WHO_AM_I QW_MPU6500_REG_WHO_AM_I
#define QW_MPU6500_I2C_TIMEOUT_MS 50

static esp_err_t read_registers(const qw_mpu6500_t *imu, uint8_t reg,
                                uint8_t *data, size_t length)
{
    if (imu == NULL || imu->transport.write_read == NULL || data == NULL ||
        length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    qw_hal_status_t status = imu->transport.write_read(
        imu->transport.ctx, &reg, 1, data, length,
        QW_MPU6500_I2C_TIMEOUT_MS);
    return status == QW_HAL_OK
               ? ESP_OK
               : status == QW_HAL_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL;
}

esp_err_t qw_mpu6500_read_register(const qw_mpu6500_t *imu, uint8_t reg,
                                   uint8_t *value)
{
    return read_registers(imu, reg, value, 1);
}

esp_err_t qw_mpu6500_write_register(const qw_mpu6500_t *imu, uint8_t reg,
                                    uint8_t value)
{
    if (imu == NULL || imu->transport.write == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[] = {reg, value};
    qw_hal_status_t status =
        imu->transport.write(imu->transport.ctx, data, sizeof(data),
                             QW_MPU6500_I2C_TIMEOUT_MS);
    return status == QW_HAL_OK
               ? ESP_OK
               : status == QW_HAL_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL;
}

esp_err_t qw_mpu6500_read_who_am_i(const qw_mpu6500_t *imu, uint8_t *value)
{
    return read_registers(imu, MPU_REG_WHO_AM_I, value, 1);
}

esp_err_t qw_mpu6500_init(i2c_master_bus_handle_t bus, uint8_t address,
                          qw_mpu6500_t *out)
{
    const qw_mpu6500_config_t config = qw_mpu6500_default_config();
    return qw_mpu6500_init_with_config(bus, address, &config, out);
}

static esp_err_t mpu_configure_with_transport(
    const qw_hal_i2c_device_t *transport, const qw_mpu6500_config_t *config,
    qw_mpu6500_t *out);

esp_err_t qw_mpu6500_init_with_config(i2c_master_bus_handle_t bus,
                                      uint8_t address,
                                      const qw_mpu6500_config_t *config,
                                      qw_mpu6500_t *out)
{
    if (bus == NULL || config == NULL || out == NULL || address > 0x7F ||
        config->accel_range > QW_MPU6500_ACCEL_RANGE_16G ||
        config->gyro_range > QW_MPU6500_GYRO_RANGE_2000DPS ||
        config->dlpf_config > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &device_config, &out->device);
    if (err != ESP_OK) {
        return err;
    }
    out->transport = qw_hal_espidf_i2c_device(out->device);

    err = mpu_configure_with_transport(&out->transport, config, out);
    if (err != ESP_OK) {
        i2c_master_bus_rm_device(out->device);
        out->device = NULL;
        out->transport = (qw_hal_i2c_device_t){0};
    }
    return err;
}

static esp_err_t mpu_configure_with_transport(
    const qw_hal_i2c_device_t *transport, const qw_mpu6500_config_t *config,
    qw_mpu6500_t *out)
{
    if (transport == NULL || transport->write == NULL ||
        transport->write_read == NULL || config == NULL || out == NULL ||
        config->accel_range > QW_MPU6500_ACCEL_RANGE_16G ||
        config->gyro_range > QW_MPU6500_GYRO_RANGE_2000DPS ||
        config->dlpf_config > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    out->transport = *transport;

    uint8_t who_am_i = 0;
    esp_err_t err = qw_mpu6500_read_who_am_i(out, &who_am_i);
    if (err != ESP_OK || who_am_i != QW_MPU6500_WHO_AM_I) {
        out->transport = (qw_hal_i2c_device_t){0};
        return err == ESP_OK ? ESP_ERR_NOT_FOUND : err;
    }

    /* Wake the device and apply the caller-selected digital filters/ranges. */
    const uint8_t setup[][2] = {
        {MPU_REG_PWR_MGMT_1, 0x00},
        {MPU_REG_CONFIG, (uint8_t)(config->dlpf_config & 0x07)},
        {MPU_REG_SMPLRT_DIV, config->sample_rate_div},
        {MPU_REG_GYRO_CONFIG, (uint8_t)(config->gyro_range << 3)},
        {MPU_REG_ACCEL_CONFIG, (uint8_t)(config->accel_range << 3)},
        {MPU_REG_ACCEL_CONFIG2, (uint8_t)(config->dlpf_config & 0x07)},
    };
    for (size_t i = 0; i < sizeof(setup) / sizeof(setup[0]); i++) {
        err = qw_mpu6500_write_register(out, setup[i][0], setup[i][1]);
        if (err != ESP_OK) {
            out->transport = (qw_hal_i2c_device_t){0};
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t qw_mpu6500_init_with_transport(
    const qw_hal_i2c_device_t *transport, const qw_mpu6500_config_t *config,
    qw_mpu6500_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    return mpu_configure_with_transport(transport, config, out);
}

esp_err_t qw_mpu6500_read_raw(const qw_mpu6500_t *imu,
                              qw_mpu6500_raw_sample_t *sample)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[14];
    esp_err_t err = read_registers(imu, MPU_REG_ACCEL_XOUT_H, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }
    sample->accel_x = (int16_t)((data[0] << 8) | data[1]);
    sample->accel_y = (int16_t)((data[2] << 8) | data[3]);
    sample->accel_z = (int16_t)((data[4] << 8) | data[5]);
    sample->temperature = (int16_t)((data[6] << 8) | data[7]);
    sample->gyro_x = (int16_t)((data[8] << 8) | data[9]);
    sample->gyro_y = (int16_t)((data[10] << 8) | data[11]);
    sample->gyro_z = (int16_t)((data[12] << 8) | data[13]);
    return ESP_OK;
}
