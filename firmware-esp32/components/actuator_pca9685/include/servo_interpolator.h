#ifndef NIULAI_SERVO_INTERPOLATOR_H
#define NIULAI_SERVO_INTERPOLATOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float position_us;
    float velocity_us_s;
    bool initialized;
} qw_servo_interpolator_t;

typedef struct {
    float max_velocity_us_s;
    float max_acceleration_us_s2;
} qw_servo_limits_t;

void qw_servo_interpolator_reset(qw_servo_interpolator_t *state,
                                 uint16_t position_us);
uint16_t qw_servo_interpolator_step(qw_servo_interpolator_t *state,
                                    uint16_t target_us, float dt_s,
                                    const qw_servo_limits_t *limits);

#endif
