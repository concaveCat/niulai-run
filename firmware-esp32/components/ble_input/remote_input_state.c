#include "remote_input_state.h"

void qw_remote_input_init(remote_leg_input_t *input)
{
    input->sequence = 0;
    input->leg_mask = 0;
    input->last_rx_time_us = 0;
    input->has_received = false;
    input->failsafe_active = false;
}

void qw_remote_input_accept(remote_leg_input_t *input, uint8_t sequence,
                            uint8_t leg_mask, int64_t now_us)
{
    input->sequence = sequence;
    input->leg_mask = leg_mask;
    input->last_rx_time_us = now_us;
    input->has_received = true;
    input->failsafe_active = false;
}

bool qw_remote_input_apply_failsafe(remote_leg_input_t *input, int64_t now_us,
                                    int64_t timeout_us)
{
    int64_t elapsed_us = now_us - input->last_rx_time_us;

    if (!input->has_received || input->failsafe_active || elapsed_us < 0 ||
        elapsed_us < timeout_us) {
        return false;
    }
    input->leg_mask = 0;
    input->failsafe_active = true;
    return true;
}

bool qw_remote_input_is_fresh(const remote_leg_input_t *input, int64_t now_us,
                              int64_t timeout_us)
{
    int64_t elapsed_us = now_us - input->last_rx_time_us;

    return input->has_received && !input->failsafe_active && elapsed_us >= 0 &&
           elapsed_us < timeout_us;
}

void qw_remote_input_clear(remote_leg_input_t *input)
{
    input->leg_mask = 0;
    input->has_received = false;
    input->failsafe_active = false;
}
