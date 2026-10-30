#ifndef QW_I2S_AUDIO_H
#define QW_I2S_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/i2s_std.h"
#include "esp_err.h"

#define QW_I2S_DEFAULT_SAMPLE_RATE_HZ 48000
#define QW_I2S_DEFAULT_SLOT_WIDTH_BITS I2S_DATA_BIT_WIDTH_32BIT

typedef struct {
    int bclk_gpio;
    int ws_gpio;
    int tx_data_gpio;
    int rx_data_gpio;
    uint32_t sample_rate_hz;
    i2s_data_bit_width_t slot_width;
    bool enable_tx;
    bool enable_rx;
} qw_i2s_audio_config_t;

typedef struct {
    i2s_chan_handle_t tx;
    i2s_chan_handle_t rx;
    bool tx_enabled;
    bool rx_enabled;
} qw_i2s_audio_t;

qw_i2s_audio_config_t qw_i2s_audio_config_default(void);
esp_err_t qw_i2s_audio_init(const qw_i2s_audio_config_t *config,
                            qw_i2s_audio_t *out);
esp_err_t qw_i2s_audio_write(qw_i2s_audio_t *audio, const void *data,
                             size_t length, size_t *written, uint32_t timeout_ms);
esp_err_t qw_i2s_audio_read(qw_i2s_audio_t *audio, void *data, size_t length,
                            size_t *read, uint32_t timeout_ms);
void qw_i2s_audio_deinit(qw_i2s_audio_t *audio);

#endif /* QW_I2S_AUDIO_H */
