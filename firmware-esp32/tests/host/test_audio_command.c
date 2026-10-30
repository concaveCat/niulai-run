#include "qw_check.h"
#include "audio_command.h"

static void test_play_commands(void)
{
    CHECK_INT_EQ(qw_audio_command_parse("audio play"),
                 QW_AUDIO_COMMAND_PLAY_MAMA);
    CHECK_INT_EQ(qw_audio_command_parse("  audio play mama\r\n"),
                 QW_AUDIO_COMMAND_PLAY_MAMA);
    CHECK_INT_EQ(qw_audio_command_parse("play"), QW_AUDIO_COMMAND_PLAY_MAMA);
    CHECK_INT_EQ(qw_audio_command_parse("play_mama_mp3\n"),
                 QW_AUDIO_COMMAND_PLAY_MAMA);
    CHECK_INT_EQ(qw_audio_command_parse("NIULAI"),
                 QW_AUDIO_COMMAND_PLAY_MAMA);
    CHECK_INT_EQ(qw_audio_command_parse("niulai"),
                 QW_AUDIO_COMMAND_PLAY_MAMA);
}

static void test_stop_commands(void)
{
    CHECK_INT_EQ(qw_audio_command_parse("audio stop"), QW_AUDIO_COMMAND_STOP);
    CHECK_INT_EQ(qw_audio_command_parse("stop audio\n"), QW_AUDIO_COMMAND_STOP);
    CHECK_INT_EQ(qw_audio_command_parse("stop_mama"), QW_AUDIO_COMMAND_STOP);
}

static void test_unrelated_commands_are_rejected(void)
{
    CHECK_INT_EQ(qw_audio_command_parse(NULL), QW_AUDIO_COMMAND_NONE);
    CHECK_INT_EQ(qw_audio_command_parse("audio playback"),
                 QW_AUDIO_COMMAND_NONE);
    CHECK_INT_EQ(qw_audio_command_parse("reset"), QW_AUDIO_COMMAND_NONE);
    CHECK_INT_EQ(qw_audio_command_parse("VOICE_COMMAND command=NIULAI"),
                 QW_AUDIO_COMMAND_NONE);
}

int main(void)
{
    RUN_TEST(test_play_commands);
    RUN_TEST(test_stop_commands);
    RUN_TEST(test_unrelated_commands_are_rejected);
    printf("test_audio_command: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
