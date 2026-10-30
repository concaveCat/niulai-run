#ifndef NIULAI_SAFETY_GATE_CONTRACT_H
#define NIULAI_SAFETY_GATE_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

/* A command may be considered committed only if the actuator transaction
 * succeeded and authorization remained unchanged for its entire transaction.
 * Keeping this predicate pure makes the revocation/epoch rule regression
 * testable without pretending a host test proves RTOS scheduling. */
static inline bool qw_safety_gate_command_commit_allowed(
    bool command_ok,
    bool armed,
    bool same_actuator,
    bool currently_authorized,
    uint32_t command_epoch,
    uint32_t current_epoch)
{
    return command_ok && armed && same_actuator && currently_authorized &&
           command_epoch == current_epoch;
}

#endif /* NIULAI_SAFETY_GATE_CONTRACT_H */
