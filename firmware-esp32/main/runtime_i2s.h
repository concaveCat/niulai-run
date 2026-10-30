#ifndef QW_RUNTIME_I2S_H
#define QW_RUNTIME_I2S_H

#include "esp_err.h"
#include "i2s_audio.h"

/* Runtime owner for the shared GPIO9/GPIO10 I2S clock domain. RX listening
 * and exclusive TX playback must not create channels concurrently. */
esp_err_t qw_runtime_i2s_acquire(const qw_i2s_audio_config_t *config,
                                 qw_i2s_audio_t *out);
void qw_runtime_i2s_release(qw_i2s_audio_t *audio);

#endif /* QW_RUNTIME_I2S_H */
