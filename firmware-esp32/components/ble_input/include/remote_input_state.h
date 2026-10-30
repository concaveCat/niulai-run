#ifndef REMOTE_INPUT_STATE_H
#define REMOTE_INPUT_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t sequence;
    uint8_t leg_mask;
    int64_t last_rx_time_us;
    bool has_received;
    bool failsafe_active;
} remote_leg_input_t;

void qw_remote_input_init(remote_leg_input_t *input);
void qw_remote_input_accept(remote_leg_input_t *input, uint8_t sequence,
                            uint8_t leg_mask, int64_t now_us);
bool qw_remote_input_apply_failsafe(remote_leg_input_t *input, int64_t now_us,
                                    int64_t timeout_us);
bool qw_remote_input_is_fresh(const remote_leg_input_t *input, int64_t now_us,
                              int64_t timeout_us);
void qw_remote_input_clear(remote_leg_input_t *input);

#endif /* REMOTE_INPUT_STATE_H */
