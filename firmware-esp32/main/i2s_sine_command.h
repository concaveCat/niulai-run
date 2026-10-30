#ifndef QW_I2S_SINE_COMMAND_H
#define QW_I2S_SINE_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    QW_I2S_SINE_COMMAND_NONE = 0,
    QW_I2S_SINE_COMMAND_TONE,
    QW_I2S_SINE_COMMAND_FREQUENCY,
    QW_I2S_SINE_COMMAND_AMPLITUDE,
    QW_I2S_SINE_COMMAND_MUTE,
    QW_I2S_SINE_COMMAND_STATUS,
    QW_I2S_SINE_COMMAND_HELP,
} qw_i2s_sine_command_kind_t;

typedef struct {
    qw_i2s_sine_command_kind_t kind;
    uint32_t frequency_hz;
    uint32_t amplitude_percent;
} qw_i2s_sine_command_t;

/* Parse one complete UART line. Frequency is 1..20000 Hz and amplitude is
 * 0..100 percent. The parser has no ESP-IDF dependency so it can be tested
 * with the host compiler. */
bool qw_i2s_sine_command_parse(const char *line,
                               qw_i2s_sine_command_t *out);

#endif /* QW_I2S_SINE_COMMAND_H */
