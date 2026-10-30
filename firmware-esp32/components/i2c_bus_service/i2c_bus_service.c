#include "i2c_bus_service.h"

#include "sdkconfig.h"

static i2c_master_bus_handle_t s_bus;
static esp_err_t s_init_result = ESP_ERR_INVALID_STATE;

esp_err_t qw_i2c_bus_service_init(void)
{
    if (s_bus != NULL) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_NIULAI_PCA9685_SDA_PIN,
        .scl_io_num = CONFIG_NIULAI_PCA9685_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    s_init_result = i2c_new_master_bus(&config, &s_bus);
    return s_init_result;
}

i2c_master_bus_handle_t qw_i2c_bus_service_bus(void)
{
    return s_bus;
}

bool qw_i2c_bus_service_ready(void)
{
    return s_bus != NULL && s_init_result == ESP_OK;
}
