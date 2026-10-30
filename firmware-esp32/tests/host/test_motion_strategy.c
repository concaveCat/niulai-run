#include <assert.h>

#include "motion_strategy.h"

static bool pressed(void *ctx, uint8_t leg)
{
    (void)ctx;
    (void)leg;
    return true;
}

static bool contact(void *ctx, uint8_t leg)
{
    (void)ctx;
    (void)leg;
    return true;
}

static void command(const qw_motion_frame_t *frame, void *ctx)
{
    leg_command_t *last = (leg_command_t *)ctx;
    *last = frame->legs[0];
}

int main(void)
{
    qw_motion_strategy_t strategy;
    leg_command_t last = {LEG_CMD_SAFE_STOP, 0.0f};
    const leg_fsm_config_t config = {
        .lift_duration_s = 0.1f,
        .swing_speed = 1.0f,
        .search_speed = 1.0f,
        .max_swing = 1.0f,
        .state_timeout_s = 1.0f,
    };
    assert(qw_motion_leg_fsm_create(&strategy));
    assert(qw_motion_strategy_valid(&strategy));
    const qw_motion_init_args_t args = {
        .strategy_config = &config,
        .leg_mask = 1U,
        .read_pressed = pressed,
        .read_contact = contact,
        .emit = command,
        .capability_ctx = &last,
    };
    assert(qw_motion_strategy_init(&strategy, &args));
    qw_motion_strategy_tick(&strategy, 0.02f);
    assert(last.mode == LEG_CMD_LIFT);
    qw_motion_snapshot_t snapshot;
    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.state == QW_MOTION_STATE_LIFT);
    qw_motion_strategy_reset(&strategy);
    assert(qw_motion_strategy_snapshot(&strategy, &snapshot));
    assert(snapshot.state == QW_MOTION_STATE_SUPPORT);
    return 0;
}
