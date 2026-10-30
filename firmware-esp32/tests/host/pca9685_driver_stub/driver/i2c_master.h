#ifndef QW_PCA9685_TEST_I2C_MASTER_H
#define QW_PCA9685_TEST_I2C_MASTER_H

#include <stddef.h>
#include <stdint.h>

typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef struct {
    int i2c_port;
    int sda_io_num;
    int scl_io_num;
    int clk_source;
    int glitch_ignore_cnt;
    struct {
        int enable_internal_pullup;
    } flags;
} i2c_master_bus_config_t;
typedef struct {
    int dev_addr_length;
    int device_address;
    int scl_speed_hz;
} i2c_device_config_t;

#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 7
#define ESP_OK 0

static inline int i2c_new_master_bus(const i2c_master_bus_config_t *config,
                                     i2c_master_bus_handle_t *bus)
{
    (void)config;
    *bus = (void *)1;
    return ESP_OK;
}

static inline int i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                            const i2c_device_config_t *config,
                                            i2c_master_dev_handle_t *device)
{
    (void)bus;
    (void)config;
    *device = (void *)1;
    return ESP_OK;
}

static inline int i2c_master_bus_rm_device(i2c_master_dev_handle_t device)
{
    (void)device;
    return ESP_OK;
}

static inline int i2c_del_master_bus(i2c_master_bus_handle_t bus)
{
    (void)bus;
    return ESP_OK;
}

#endif
