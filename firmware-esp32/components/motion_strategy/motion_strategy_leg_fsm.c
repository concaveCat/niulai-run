#include "motion_strategy.h"

#include <string.h>

#include "control_glue.h"

typedef struct {
    leg_runner_t runner;
    qw_motion_init_args_t args;
    uint8_t leg_index;
} qw_leg_fsm_adapter_t;

static qw_leg_fsm_adapter_t s_adapter;

static uint8_t first_leg(uint8_t mask)
{
    for (uint8_t i = 0; i < QW_MOTION_MAX_LEGS; i++) {
        if ((mask & (uint8_t)(1U << i)) != 0U) {
            return i;
        }
    }
    return 0U;
}

static bool read_pressed(void *ctx)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    return adapter->args.read_pressed(adapter->args.capability_ctx,
                                      adapter->leg_index);
}

static bool read_contact(void *ctx)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    return adapter->args.read_contact(adapter->args.capability_ctx,
                                      adapter->leg_index);
}

static void emit_command(const leg_command_t *command, void *ctx)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    qw_motion_frame_t frame = {0};
    frame.command_mask = (uint8_t)(1U << adapter->leg_index);
    frame.legs[adapter->leg_index] = *command;
    adapter->args.emit(&frame, adapter->args.capability_ctx);
}

static bool motion_leg_fsm_init(void *ctx, const qw_motion_init_args_t *args)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    if (adapter == NULL || args == NULL || args->strategy_config == NULL ||
        args->leg_mask == 0U || args->read_pressed == NULL ||
        args->read_contact == NULL || args->emit == NULL) {
        return false;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->args = *args;
    adapter->leg_index = first_leg(args->leg_mask);
    return leg_runner_init(&adapter->runner,
                           (const leg_fsm_config_t *)args->strategy_config,
                           read_pressed, read_contact, emit_command, adapter);
}

static void motion_leg_fsm_tick(void *ctx, float dt_s)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    if (adapter != NULL) {
        leg_runner_tick(&adapter->runner, dt_s);
    }
}

static qw_motion_state_code_t state_code(leg_state_t state)
{
    switch (state) {
    case LEG_STATE_SUPPORT:
        return QW_MOTION_STATE_SUPPORT;
    case LEG_STATE_LIFT:
        return QW_MOTION_STATE_LIFT;
    case LEG_STATE_SWING:
        return QW_MOTION_STATE_SWING;
    case LEG_STATE_SEARCH:
        return QW_MOTION_STATE_SEARCH;
    case LEG_STATE_FAULT:
        return QW_MOTION_STATE_FAULT;
    default:
        return QW_MOTION_STATE_UNKNOWN;
    }
}

static bool motion_leg_fsm_snapshot(void *ctx, qw_motion_snapshot_t *out)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    if (adapter == NULL || out == NULL) {
        return false;
    }
    const leg_fsm_t *fsm = &adapter->runner.fsm;
    memset(out, 0, sizeof(*out));
    out->strategy_name = "leg_fsm";
    out->state_name = leg_state_name(fsm->state);
    out->state = state_code(fsm->state);
    out->fault = fsm->state == LEG_STATE_FAULT;
    out->transition_total = fsm->transition_total;
    out->transition_count = leg_fsm_transition_count(fsm);
    for (uint32_t i = 0; i < out->transition_count; i++) {
        const leg_transition_t *transition = leg_fsm_transition_get(fsm, i);
        if (transition == NULL) {
            return false;
        }
        out->transitions[i] = (qw_motion_transition_t){
            .from_state = leg_state_name(transition->from_state),
            .to_state = leg_state_name(transition->to_state),
            .reason = transition->reason,
            .elapsed_s = transition->elapsed_s,
        };
    }
    return true;
}

static void motion_leg_fsm_reset(void *ctx)
{
    qw_leg_fsm_adapter_t *adapter = (qw_leg_fsm_adapter_t *)ctx;
    if (adapter != NULL) {
        leg_fsm_reset(&adapter->runner.fsm);
    }
}

bool qw_motion_leg_fsm_create(qw_motion_strategy_t *strategy)
{
    if (strategy == NULL) {
        return false;
    }
    *strategy = (qw_motion_strategy_t){
        .ctx = &s_adapter,
        .init = motion_leg_fsm_init,
        .tick = motion_leg_fsm_tick,
        .snapshot = motion_leg_fsm_snapshot,
        .reset = motion_leg_fsm_reset,
    };
    return true;
}
