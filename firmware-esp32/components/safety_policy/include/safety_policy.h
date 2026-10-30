#ifndef QW_SAFETY_POLICY_H
#define QW_SAFETY_POLICY_H

#include <stdbool.h>

typedef struct {
    bool actuation_enabled;
    bool acceptance_ok;
    bool actuator_ready;
    bool control_ready;
    bool contact_ready;
    bool ble_link_secure;
    bool stop_latched;
    bool failsafe;
    bool motion_fault;
    bool actuator_fault;
} qw_safety_policy_input_t;

/* Pure authorization predicate. It does not touch /OE, a bus, or an
 * actuator. The runtime safety gate remains responsible for the safe-open
 * and safe-close ordering around this decision. */
bool qw_safety_policy_should_arm(qw_safety_policy_input_t input);

#endif /* QW_SAFETY_POLICY_H */
