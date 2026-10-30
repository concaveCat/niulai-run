#include "i2s_audio.h"

#include <string.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

qw_i2s_audio_config_t qw_i2s_audio_config_default(void)
{
    return (qw_i2s_audio_config_t){
        .bclk_gpio = CONFIG_NIULAI_I2S_BCLK_GPIO,
        .ws_gpio = CONFIG_NIULAI_I2S_WS_GPIO,
        .tx_data_gpio = CONFIG_NIULAI_I2S_TX_GPIO,
        .rx_data_gpio = CONFIG_NIULAI_I2S_RX_GPIO,
        .sample_rate_hz = QW_I2S_DEFAULT_SAMPLE_RATE_HZ,
        .slot_width = QW_I2S_DEFAULT_SLOT_WIDTH_BITS,
        .enable_tx = true,
        .enable_rx = true,
    };
}

esp_err_t qw_i2s_audio_init(const qw_i2s_audio_config_t *config,
                            qw_i2s_audio_t *out)
{
    if (config == NULL || out == NULL || config->bclk_gpio < 0 ||
        config->ws_gpio < 0 || config->sample_rate_hz == 0 ||
        (!config->enable_tx && !config->enable_rx)) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    /* The ESP32-S3 voice profile uses one shared I2S bus for the MS4030 RX
     * and MAX98357A TX paths.  Pin the channel to I2S0: the IDF 5.5 AUTO
     * allocator can enter the GDMA shared-interrupt lookup with an incomplete
     * vector descriptor on this board. */
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;
    esp_err_t err = i2s_new_channel(
        &channel_config, config->enable_tx ? &out->tx : NULL,
        config->enable_rx ? &out->rx : NULL);
    if (err != ESP_OK) {
        return err;
    }

    i2s_std_config_t standard_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            config->slot_width, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->bclk_gpio,
            .ws = config->ws_gpio,
            .dout = config->enable_tx ? config->tx_data_gpio : I2S_GPIO_UNUSED,
            .din = config->enable_rx ? config->rx_data_gpio : I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    if (config->enable_tx) {
        err = i2s_channel_init_std_mode(out->tx, &standard_config);
    }
    if (err == ESP_OK && config->enable_rx) {
        err = i2s_channel_init_std_mode(out->rx, &standard_config);
    }
    if (err == ESP_OK && config->enable_tx) {
        err = i2s_channel_enable(out->tx);
        out->tx_enabled = err == ESP_OK;
    }
    if (err == ESP_OK && config->enable_rx) {
        err = i2s_channel_enable(out->rx);
        out->rx_enabled = err == ESP_OK;
    }
    if (err != ESP_OK) {
        qw_i2s_audio_deinit(out);
    }
    return err;
}

esp_err_t qw_i2s_audio_write(qw_i2s_audio_t *audio, const void *data,
                             size_t length, size_t *written, uint32_t timeout_ms)
{
    if (audio == NULL || audio->tx == NULL || data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2s_channel_write(audio->tx, data, length, written,
                             pdMS_TO_TICKS(timeout_ms));
}

esp_err_t qw_i2s_audio_read(qw_i2s_audio_t *audio, void *data, size_t length,
                            size_t *read, uint32_t timeout_ms)
{
    if (audio == NULL || audio->rx == NULL || data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2s_channel_read(audio->rx, data, length, read,
                            pdMS_TO_TICKS(timeout_ms));
}

void qw_i2s_audio_deinit(qw_i2s_audio_t *audio)
{
    if (audio == NULL) {
        return;
    }
    if (audio->tx_enabled) {
        i2s_channel_disable(audio->tx);
    }
    if (audio->rx_enabled) {
        i2s_channel_disable(audio->rx);
    }
    /* TX and RX are separate handles when both directions were requested. */
    if (audio->tx != NULL) {
        i2s_del_channel(audio->tx);
    }
    if (audio->rx != NULL) {
        i2s_del_channel(audio->rx);
    }
    memset(audio, 0, sizeof(*audio));
}
