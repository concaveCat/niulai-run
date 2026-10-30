#include "safety_policy.h"

bool qw_safety_policy_should_arm(qw_safety_policy_input_t input)
{
    return input.actuation_enabled && input.acceptance_ok &&
           input.actuator_ready && input.control_ready && input.contact_ready &&
           input.ble_link_secure && !input.stop_latched && !input.failsafe &&
           !input.motion_fault && !input.actuator_fault;
}
