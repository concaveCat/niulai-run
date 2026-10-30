#ifndef QW_AUDIO_COMMAND_H
#define QW_AUDIO_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QW_AUDIO_COMMAND_NONE = 0,
    QW_AUDIO_COMMAND_PLAY_MAMA,
    QW_AUDIO_COMMAND_STOP,
} qw_audio_command_t;

/* Parses one trimmed UART command. This file is dependency-free so the
 * command contract can be tested on the host without ESP-IDF. */
qw_audio_command_t qw_audio_command_parse(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* QW_AUDIO_COMMAND_H */
