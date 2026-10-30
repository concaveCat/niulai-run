#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "servo_interpolator.h"

int main(void)
{
    const float dt_s = 0.01f;
    const qw_servo_limits_t limits = {
        .max_velocity_us_s = 900.0f,
        .max_acceleration_us_s2 = 3600.0f,
    };
    qw_servo_interpolator_t state = {0};
    qw_servo_interpolator_reset(&state, 1500u);

    float previous_position = state.position_us;
    float previous_velocity = state.velocity_us_s;
    uint16_t output = 1500u;
    for (uint32_t tick = 0u; tick < 300u; ++tick) {
        output = qw_servo_interpolator_step(&state, 900u, dt_s, &limits);
        assert(output >= 900u && output <= 1500u);
        assert(fabsf(state.velocity_us_s) <= limits.max_velocity_us_s + 0.01f);
        if (output != 900u) {
            assert(fabsf(state.velocity_us_s - previous_velocity) <=
                   limits.max_acceleration_us_s2 * dt_s + 0.01f);
        }
        assert(fabsf(state.position_us - previous_position) <=
               limits.max_velocity_us_s * dt_s + 0.01f);
        previous_position = state.position_us;
        previous_velocity = state.velocity_us_s;
    }
    assert(output == 900u && state.velocity_us_s == 0.0f);

    /* A sudden reversal is decelerated; it cannot become a target jump. */
    for (uint32_t tick = 0u; tick < 10u; ++tick) {
        (void)qw_servo_interpolator_step(&state, 1800u, dt_s, &limits);
    }
    previous_position = state.position_us;
    previous_velocity = state.velocity_us_s;
    (void)qw_servo_interpolator_step(&state, 700u, dt_s, &limits);
    assert(fabsf(state.velocity_us_s - previous_velocity) <=
           limits.max_acceleration_us_s2 * dt_s + 0.01f);
    assert(fabsf(state.position_us - previous_position) <=
           limits.max_velocity_us_s * dt_s + 0.01f);

    /* Invalid timing is fail-transparent to the requested target. */
    assert(qw_servo_interpolator_step(&state, 1234u, 0.0f, &limits) == 1234u);
    return 0;
}
