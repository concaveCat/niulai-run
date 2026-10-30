#include "audio_command.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int command_equals(const char *line, const char *expected)
{
    if (line == NULL || expected == NULL) {
        return 0;
    }

    while (isspace((unsigned char)*line)) {
        ++line;
    }

    size_t line_length = strlen(line);
    while (line_length > 0U &&
           isspace((unsigned char)line[line_length - 1U])) {
        --line_length;
    }

    const size_t expected_length = strlen(expected);
    return line_length == expected_length &&
           memcmp(line, expected, expected_length) == 0;
}

qw_audio_command_t qw_audio_command_parse(const char *line)
{
    if (command_equals(line, "audio play") ||
        command_equals(line, "audio play mama") ||
        command_equals(line, "play") ||
        command_equals(line, "play mama") ||
        command_equals(line, "play_mama") ||
        command_equals(line, "play_mama_mp3") ||
        command_equals(line, "NIULAI") ||
        command_equals(line, "niulai")) {
        return QW_AUDIO_COMMAND_PLAY_MAMA;
    }

    if (command_equals(line, "audio stop") ||
        command_equals(line, "audio stop mama") ||
        command_equals(line, "stop audio") ||
        command_equals(line, "stop_mama")) {
        return QW_AUDIO_COMMAND_STOP;
    }

    return QW_AUDIO_COMMAND_NONE;
}
