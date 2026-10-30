/* Host tests for the pure-C control glue (components/control_glue).
 * Mirrors the Python controller test test_controller_uses_only_replaceable
 * _capabilities plus init validation. */
#include "qw_check.h"
#include "control_glue.h"
#include "leg_fsm.h"

typedef struct {
    bool pressed;
    bool contact;
    leg_command_t commands[8];
    int command_count;
} mock_world_t;

static bool mock_read_pressed(void *ctx)
{
    return ((mock_world_t *)ctx)->pressed;
}

static bool mock_read_contact(void *ctx)
{
    return ((mock_world_t *)ctx)->contact;
}

static void mock_command_leg(const leg_command_t *cmd, void *ctx)
{
    mock_world_t *w = (mock_world_t *)ctx;
    if (w->command_count < (int)(sizeof(w->commands) / sizeof(w->commands[0]))) {
        w->commands[w->command_count++] = *cmd;
    }
}

/* mirrors Python test_controller_uses_only_replaceable_capabilities */
static void test_runner_uses_only_replaceable_capabilities(void)
{
    mock_world_t world = {0};
    leg_runner_t runner;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.0f};

    CHECK(leg_runner_init(&runner, &cfg, mock_read_pressed,
                          mock_read_contact, mock_command_leg, &world));

    leg_runner_tick(&runner, 0.0f); /* SUPPORT -> hold_support */
    world.pressed = true;
    leg_runner_tick(&runner, 0.01f); /* -> LIFT */
    leg_runner_tick(&runner, 0.1f);  /* elapsed 0.11 >= 0.1 -> SWING */

    CHECK_INT_EQ(world.command_count, 3);
    CHECK(world.commands[2].mode == LEG_CMD_SWING);
}

/* every tick produces exactly one command, routed through the callback */
static void test_runner_ticks_route_commands(void)
{
    mock_world_t world = {0};
    leg_runner_t runner;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.0f};

    CHECK(leg_runner_init(&runner, &cfg, mock_read_pressed,
                          mock_read_contact, mock_command_leg, &world));

    leg_runner_tick(&runner, 0.0f);
    CHECK_INT_EQ(world.command_count, 1);
    CHECK(world.commands[0].mode == LEG_CMD_HOLD_SUPPORT);

    world.contact = true;
    leg_runner_tick(&runner, 0.0f);
    CHECK_INT_EQ(world.command_count, 2);
    CHECK(world.commands[1].mode == LEG_CMD_HOLD_SUPPORT); /* still SUPPORT */
}

static void test_runner_init_rejects_invalid_setup(void)
{
    leg_runner_t runner;
    leg_fsm_config_t bad_cfg = {-0.1f, 1.0f, 0.5f, 1.0f, 0.0f};
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.0f};

    CHECK(!leg_runner_init(&runner, &bad_cfg, mock_read_pressed,
                           mock_read_contact, mock_command_leg, &runner));
    CHECK(!leg_runner_init(&runner, &cfg, NULL, mock_read_contact,
                           mock_command_leg, &runner));
    CHECK(!leg_runner_init(&runner, &cfg, mock_read_pressed, NULL,
                           mock_command_leg, &runner));
    CHECK(!leg_runner_init(&runner, &cfg, mock_read_pressed,
                           mock_read_contact, NULL, &runner));
}

int main(void)
{
    RUN_TEST(test_runner_uses_only_replaceable_capabilities);
    RUN_TEST(test_runner_ticks_route_commands);
    RUN_TEST(test_runner_init_rejects_invalid_setup);
    printf("test_control_glue: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
