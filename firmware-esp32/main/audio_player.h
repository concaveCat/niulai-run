#ifndef QW_AUDIO_PLAYER_H
#define QW_AUDIO_PLAYER_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the asynchronous player task. Playback starts only after a play
 * command or an explicit event calls qw_audio_player_play_mama(). */
esp_err_t qw_audio_player_init(void);

/* Starts the embedded /牛来叫妈妈.mp3 from the beginning. If it is already
 * playing, the current stream is replaced by a fresh playback request. */
esp_err_t qw_audio_player_play_mama(void);

/* Plays the embedded asset synchronously for an exclusive runtime or
 * diagnostic caller that owns the I2S peripheral. */
esp_err_t qw_audio_player_play_mama_blocking(void);

/* Requests a stop at the next decoder/I2S chunk boundary. */
esp_err_t qw_audio_player_stop(void);

/* Handles the canonical UART command and its documented machine aliases.
 * Returns true when the line is an audio command, including on an execution
 * error; callers can then avoid reporting it as an unknown command. */
bool qw_audio_player_handle_command(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* QW_AUDIO_PLAYER_H */
