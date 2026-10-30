#include "foot_contact_debounce.h"

#include <stddef.h>

void foot_contact_debounce_init(foot_contact_debounce_t *state,
                                bool initial_level)
{
    if (state == NULL) {
        return;
    }
    state->debounced = initial_level;
    state->agree_count = 0;
}

bool foot_contact_debounce_update(foot_contact_debounce_t *state,
                                  bool raw_level, uint32_t threshold)
{
    if (state == NULL) {
        return false;
    }
    if (threshold == 0) {
        threshold = 1;
    }
    if (raw_level == state->debounced) {
        state->agree_count = 0;
    } else if (state->agree_count < threshold) {
        state->agree_count++;
        if (state->agree_count >= threshold) {
            state->debounced = raw_level;
            state->agree_count = 0;
        }
    }
    return state->debounced;
}
