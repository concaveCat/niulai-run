#include "qw_check.h"
#include "i2s_sine_command.h"

static qw_i2s_sine_command_t parse(const char *line)
{
    qw_i2s_sine_command_t command;
    CHECK(qw_i2s_sine_command_parse(line, &command));
    return command;
}

static void test_tone_and_partial_updates(void)
{
    qw_i2s_sine_command_t command = parse(" tone 1000 5\r\n");
    CHECK_INT_EQ(command.kind, QW_I2S_SINE_COMMAND_TONE);
    CHECK_INT_EQ(command.frequency_hz, 1000);
    CHECK_INT_EQ(command.amplitude_percent, 5);

    command = parse("freq 440");
    CHECK_INT_EQ(command.kind, QW_I2S_SINE_COMMAND_FREQUENCY);
    CHECK_INT_EQ(command.frequency_hz, 440);

    command = parse("volume 20");
    CHECK_INT_EQ(command.kind, QW_I2S_SINE_COMMAND_AMPLITUDE);
    CHECK_INT_EQ(command.amplitude_percent, 20);

    command = parse("amp 0");
    CHECK_INT_EQ(command.amplitude_percent, 0);
}

static void test_control_commands(void)
{
    qw_i2s_sine_command_t command = parse("mute");
    CHECK_INT_EQ(command.kind, QW_I2S_SINE_COMMAND_MUTE);
    CHECK(qw_i2s_sine_command_parse("status", &command));
    CHECK_INT_EQ(command.kind, QW_I2S_SINE_COMMAND_STATUS);
    CHECK(qw_i2s_sine_command_parse("help\n", &command));
    CHECK_INT_EQ(command.kind, QW_I2S_SINE_COMMAND_HELP);
}

static void test_invalid_ranges_and_extra_arguments(void)
{
    qw_i2s_sine_command_t command;
    CHECK(!qw_i2s_sine_command_parse(NULL, &command));
    CHECK(!qw_i2s_sine_command_parse("tone 0 5", &command));
    CHECK(!qw_i2s_sine_command_parse("tone 20001 5", &command));
    CHECK(!qw_i2s_sine_command_parse("tone 1000 101", &command));
    CHECK(!qw_i2s_sine_command_parse("amp -1", &command));
    CHECK(!qw_i2s_sine_command_parse("freq 1000 extra", &command));
    CHECK(!qw_i2s_sine_command_parse("mute 1", &command));
    CHECK(!qw_i2s_sine_command_parse("unknown", &command));
}

int main(void)
{
    RUN_TEST(test_tone_and_partial_updates);
    RUN_TEST(test_control_commands);
    RUN_TEST(test_invalid_ranges_and_extra_arguments);
    printf("test_i2s_sine_command: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
