#include <assert.h>
#include <stdbool.h>

#include "pca9685_safety.h"
#include "control_admission.h"
#include "safety_gate_contract.h"

typedef struct {
    unsigned calls[3];
    bool result[3];
} stop_fake_t;

static bool stop_step(void *ctx, unsigned index)
{
    stop_fake_t *fake = ctx;
    fake->calls[index]++;
    return fake->result[index];
}

static bool oe_step(void *ctx)
{
    return stop_step(ctx, 0U);
}

static bool rest_step(void *ctx)
{
    return stop_step(ctx, 1U);
}

static bool sleep_step(void *ctx)
{
    return stop_step(ctx, 2U);
}

static void assert_all_stop_steps_run(bool oe, bool rest, bool sleep,
                                      bool expected)
{
    stop_fake_t fake = {
        .result = {oe, rest, sleep},
    };
    assert(qw_pca9685_run_safe_stop_steps(oe_step, rest_step, sleep_step,
                                          &fake) == expected);
    assert(fake.calls[0] == 1U);
    assert(fake.calls[1] == 1U);
    assert(fake.calls[2] == 1U);
}

int main(void)
{
    for (unsigned mask = 0U; mask < 8U; mask++) {
        const bool oe = (mask & 1U) != 0U;
        const bool rest = (mask & 2U) != 0U;
        const bool sleep = (mask & 4U) != 0U;
        assert_all_stop_steps_run(oe, rest, sleep, oe && rest && sleep);
    }

    assert(qw_safety_gate_command_commit_allowed(true, true, true, true,
                                                 7U, 7U));
    assert(!qw_safety_gate_command_commit_allowed(true, true, true, true,
                                                  7U, 8U));
    assert(!qw_safety_gate_command_commit_allowed(true, true, true, false,
                                                  7U, 7U));
    assert(!qw_safety_gate_command_commit_allowed(false, true, true, true,
                                                  7U, 7U));

    qw_control_admission_t admission = {0};
    qw_control_admission_result_t result =
        qw_control_admission_step(&admission, false, 0x01U);
    assert(!result.reset_fsm && !result.allow_tick);
    result = qw_control_admission_step(&admission, true, 0x01U);
    assert(result.reset_fsm && !result.allow_tick);
    result = qw_control_admission_step(&admission, true, 0x01U);
    assert(!result.reset_fsm && !result.allow_tick);
    result = qw_control_admission_step(&admission, true, 0x00U);
    assert(!result.reset_fsm && !result.allow_tick);
    result = qw_control_admission_step(&admission, true, 0x01U);
    assert(!result.reset_fsm && result.allow_tick);
    result = qw_control_admission_step(&admission, false, 0x01U);
    assert(result.reset_fsm && !result.allow_tick);
    result = qw_control_admission_step(&admission, true, 0x01U);
    assert(result.reset_fsm && !result.allow_tick);
    return 0;
}
