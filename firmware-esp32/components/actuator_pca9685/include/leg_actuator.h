/* Leg actuator capability: replaceable function table, per the plan.
 * Two implementations: qw_actuator_null (log-only, default) and
 * qw_actuator_pca9685 (I2C servo driver). The safety gate is the only
 * code allowed to swap between them or lower /OE. */
#ifndef LEG_ACTUATOR_H
#define LEG_ACTUATOR_H

#include <stdbool.h>

#include "motion_strategy.h"

typedef struct {
    /* Idempotent. Must leave /OE high and the chip in SLEEP. */
    bool (*init)(void);
    /* Write a replaceable strategy frame (pulse targets only - never /OE). */
    bool (*command_frame)(const qw_motion_frame_t *frame);
    /* /OE high + neutral values + SLEEP. Always safe. */
    bool (*force_safe_stop)(void);
    /* Raw /OE level; true = high = outputs disabled. */
    bool (*set_oe)(bool level);
} leg_actuator_t;

extern const leg_actuator_t qw_actuator_null;
extern const leg_actuator_t qw_actuator_pca9685;

#endif /* LEG_ACTUATOR_H */
