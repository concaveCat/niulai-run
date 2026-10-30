#ifndef QW_VOICE_KEYWORD_TEST_H
#define QW_VOICE_KEYWORD_TEST_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Isolated MS4030/I2S keyword test entry point. */
esp_err_t qw_voice_keyword_test_start(void);

/* Normal-runtime always-on listener. A match requests only the embedded
 * mama audio action and never a motion command. */
esp_err_t qw_voice_keyword_runtime_start(void);
bool qw_voice_keyword_runtime_handle_command(const char *line);
void qw_voice_keyword_test_request_enroll(uint8_t speaker_index);
void qw_voice_keyword_test_request_play(void);
void qw_voice_keyword_test_request_capture(void);
void qw_voice_keyword_test_request_raw(void);
void qw_voice_keyword_test_clear(void);
void qw_voice_keyword_test_status(void);

#endif /* QW_VOICE_KEYWORD_TEST_H */
