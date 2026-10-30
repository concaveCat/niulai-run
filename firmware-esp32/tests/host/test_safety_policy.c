#include <assert.h>
#include <stddef.h>

#include "safety_policy.h"

static qw_safety_policy_input_t valid_input(void)
{
    return (qw_safety_policy_input_t){
        .actuation_enabled = true,
        .acceptance_ok = true,
        .actuator_ready = true,
        .control_ready = true,
        .contact_ready = true,
        .ble_link_secure = true,
        .stop_latched = false,
        .failsafe = false,
        .motion_fault = false,
        .actuator_fault = false,
    };
}

int main(void)
{
    qw_safety_policy_input_t input = valid_input();
    assert(qw_safety_policy_should_arm(input));
    bool *positive_conditions[] = {
        &input.actuation_enabled, &input.acceptance_ok,
        &input.actuator_ready,    &input.control_ready, &input.contact_ready,
    };
    for (size_t i = 0;
         i < sizeof(positive_conditions) / sizeof(positive_conditions[0]);
         i++) {
        input = valid_input();
        *positive_conditions[i] = false;
        assert(!qw_safety_policy_should_arm(input));
    }
    input = valid_input();
    input.ble_link_secure = false;
    assert(!qw_safety_policy_should_arm(input));
    input = valid_input();
    input.stop_latched = true;
    assert(!qw_safety_policy_should_arm(input));
    input = valid_input();
    input.failsafe = true;
    assert(!qw_safety_policy_should_arm(input));
    input = valid_input();
    input.motion_fault = true;
    assert(!qw_safety_policy_should_arm(input));
    input = valid_input();
    input.actuator_fault = true;
    assert(!qw_safety_policy_should_arm(input));
    return 0;
}
