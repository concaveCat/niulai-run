#include "mpu6500_probe_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "mpu6500.h"
#include "sdkconfig.h"

#define QW_MPU6500_PROBE_ADDRESS_LOW 0x68U
#define QW_MPU6500_PROBE_ADDRESS_HIGH 0x69U
#define QW_MPU6500_PROBE_WHO_AM_I_REG 0x75U
#define QW_MPU6500_PROBE_RAW_REG 0x3BU
#define QW_MPU6500_PROBE_EXPECTED_WHO_AM_I 0x70U
#define QW_MPU6500_PROBE_I2C_HZ 400000U
#define QW_MPU6500_PROBE_TIMEOUT_MS 100

static const char *TAG = "mpu6500_probe";

static esp_err_t read_register(i2c_master_dev_handle_t device, uint8_t reg,
                               uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(device, &reg, 1U, data, length,
                                       QW_MPU6500_PROBE_TIMEOUT_MS);
}

static int16_t decode_be_i16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

esp_err_t qw_mpu6500_probe_test(void)
{
    const uint8_t addresses[] = {
        QW_MPU6500_PROBE_ADDRESS_LOW,
        QW_MPU6500_PROBE_ADDRESS_HIGH,
    };
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_NIULAI_PCA9685_SDA_PIN,
        .scl_io_num = CONFIG_NIULAI_PCA9685_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    printf("MPU6500_PROBE_CONFIG sda_gpio=%d scl_gpio=%d i2c_hz=%u "
           "addresses=0x%02x,0x%02x expected_who_am_i=0x%02x\n",
           CONFIG_NIULAI_PCA9685_SDA_PIN,
           CONFIG_NIULAI_PCA9685_SCL_PIN,
           (unsigned)QW_MPU6500_PROBE_I2C_HZ,
           (unsigned)QW_MPU6500_PROBE_ADDRESS_LOW,
           (unsigned)QW_MPU6500_PROBE_ADDRESS_HIGH,
           (unsigned)QW_MPU6500_PROBE_EXPECTED_WHO_AM_I);
    fflush(stdout);

    bool found = false;
    bool id_ok = false;
    bool raw_read_ok = false;
    uint8_t found_address = 0U;
    uint8_t found_who_am_i = 0U;

    for (size_t index = 0U; index < sizeof(addresses); ++index) {
        const uint8_t address = addresses[index];
        err = i2c_master_probe(bus, address, QW_MPU6500_PROBE_TIMEOUT_MS);
        if (err != ESP_OK) {
            printf("MPU6500_PROBE address=0x%02x ack=0 error=%s\n",
                   (unsigned)address, esp_err_to_name(err));
            continue;
        }

        found = true;
        found_address = address;
        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = QW_MPU6500_PROBE_I2C_HZ,
        };
        i2c_master_dev_handle_t device = NULL;
        err = i2c_master_bus_add_device(bus, &device_config, &device);
        if (err != ESP_OK) {
            printf("MPU6500_PROBE address=0x%02x ack=1 device_add=error:%s\n",
                   (unsigned)address, esp_err_to_name(err));
            continue;
        }

        uint8_t who_am_i = 0U;
        err = read_register(device, QW_MPU6500_PROBE_WHO_AM_I_REG, &who_am_i,
                            1U);
        if (err != ESP_OK) {
            printf("MPU6500_PROBE address=0x%02x ack=1 who_am_i=read_error:%s\n",
                   (unsigned)address, esp_err_to_name(err));
            i2c_master_bus_rm_device(device);
            continue;
        }

        found_who_am_i = who_am_i;
        id_ok = who_am_i == QW_MPU6500_PROBE_EXPECTED_WHO_AM_I;
        printf("MPU6500_PROBE address=0x%02x ack=1 who_am_i=0x%02x id_ok=%u\n",
               (unsigned)address, (unsigned)who_am_i, id_ok ? 1U : 0U);

        uint8_t raw[14] = {0};
        err = read_register(device, QW_MPU6500_PROBE_RAW_REG, raw,
                            sizeof(raw));
        if (err == ESP_OK) {
            raw_read_ok = true;
            printf("MPU6500_RAW address=0x%02x accel=%d,%d,%d temp=%d "
                   "gyro=%d,%d,%d\n",
                   (unsigned)address, (int)decode_be_i16(&raw[0]),
                   (int)decode_be_i16(&raw[2]), (int)decode_be_i16(&raw[4]),
                   (int)decode_be_i16(&raw[6]), (int)decode_be_i16(&raw[8]),
                   (int)decode_be_i16(&raw[10]),
                   (int)decode_be_i16(&raw[12]));
        } else {
            printf("MPU6500_RAW address=0x%02x read_error=%s\n",
                   (unsigned)address, esp_err_to_name(err));
        }

        i2c_master_bus_rm_device(device);
        if (id_ok) {
            break;
        }
    }

    printf("MPU6500_PROBE_RESULT found=%u address=0x%02x who_am_i=0x%02x "
           "id_ok=%u raw_read_ok=%u\n",
           found ? 1U : 0U, (unsigned)found_address,
           (unsigned)found_who_am_i, id_ok ? 1U : 0U,
           raw_read_ok ? 1U : 0U);
    printf("MPU6500_PROBE_%s\n", id_ok ? "PASS" : "FAIL");
    fflush(stdout);

    esp_err_t cleanup_err = i2c_del_master_bus(bus);
    if (cleanup_err != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus cleanup failed: %s",
                 esp_err_to_name(cleanup_err));
    }
    return id_ok ? ESP_OK : (found ? ESP_ERR_INVALID_RESPONSE
                                   : ESP_ERR_NOT_FOUND);
}
