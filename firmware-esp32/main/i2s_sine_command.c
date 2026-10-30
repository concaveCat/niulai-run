#include "i2s_sine_command.h"

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define QW_I2S_SINE_COMMAND_LINE_MAX 96U
#define QW_I2S_SINE_MIN_FREQUENCY_HZ 1U
#define QW_I2S_SINE_MAX_FREQUENCY_HZ 20000U
#define QW_I2S_SINE_MAX_AMPLITUDE_PERCENT 100U

static bool parse_uint32(const char *text, uint32_t minimum,
                         uint32_t maximum, uint32_t *out)
{
    if (text == NULL || *text == '\0' || out == NULL) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || value > UINT32_MAX) {
        return false;
    }
    while (isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0' || value < minimum || value > maximum) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool no_more_tokens(void)
{
    return strtok(NULL, " \t\r\n") == NULL;
}

bool qw_i2s_sine_command_parse(const char *line,
                               qw_i2s_sine_command_t *out)
{
    if (line == NULL || out == NULL) {
        return false;
    }

    size_t length = strlen(line);
    if (length >= QW_I2S_SINE_COMMAND_LINE_MAX) {
        return false;
    }

    char buffer[QW_I2S_SINE_COMMAND_LINE_MAX];
    memcpy(buffer, line, length + 1U);
    char *command = strtok(buffer, " \t\r\n");
    char *first = strtok(NULL, " \t\r\n");
    char *second = strtok(NULL, " \t\r\n");

    out->kind = QW_I2S_SINE_COMMAND_NONE;
    out->frequency_hz = 0U;
    out->amplitude_percent = 0U;

    if (command == NULL) {
        return false;
    }

    if (strcmp(command, "tone") == 0) {
        if (first == NULL || second == NULL || !no_more_tokens() ||
            !parse_uint32(first, QW_I2S_SINE_MIN_FREQUENCY_HZ,
                          QW_I2S_SINE_MAX_FREQUENCY_HZ,
                          &out->frequency_hz) ||
            !parse_uint32(second, 0U, QW_I2S_SINE_MAX_AMPLITUDE_PERCENT,
                          &out->amplitude_percent)) {
            return false;
        }
        out->kind = QW_I2S_SINE_COMMAND_TONE;
        return true;
    }

    if (strcmp(command, "freq") == 0) {
        if (first == NULL || second != NULL || !no_more_tokens() ||
            !parse_uint32(first, QW_I2S_SINE_MIN_FREQUENCY_HZ,
                          QW_I2S_SINE_MAX_FREQUENCY_HZ,
                          &out->frequency_hz)) {
            return false;
        }
        out->kind = QW_I2S_SINE_COMMAND_FREQUENCY;
        return true;
    }

    if (strcmp(command, "amp") == 0 || strcmp(command, "volume") == 0) {
        if (first == NULL || second != NULL || !no_more_tokens() ||
            !parse_uint32(first, 0U, QW_I2S_SINE_MAX_AMPLITUDE_PERCENT,
                          &out->amplitude_percent)) {
            return false;
        }
        out->kind = QW_I2S_SINE_COMMAND_AMPLITUDE;
        return true;
    }

    if (strcmp(command, "mute") == 0 || strcmp(command, "stop") == 0) {
        if (first != NULL || second != NULL || !no_more_tokens()) {
            return false;
        }
        out->kind = QW_I2S_SINE_COMMAND_MUTE;
        return true;
    }

    if (strcmp(command, "status") == 0) {
        if (first != NULL || second != NULL || !no_more_tokens()) {
            return false;
        }
        out->kind = QW_I2S_SINE_COMMAND_STATUS;
        return true;
    }

    if (strcmp(command, "help") == 0) {
        if (first != NULL || second != NULL || !no_more_tokens()) {
            return false;
        }
        out->kind = QW_I2S_SINE_COMMAND_HELP;
        return true;
    }

    return false;
}
