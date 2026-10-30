/* Hardware-independent contact debounce state. */
#ifndef FOOT_CONTACT_DEBOUNCE_H
#define FOOT_CONTACT_DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool debounced;
    uint32_t agree_count;
} foot_contact_debounce_t;

/* Initializes an input at the supplied debounced level. */
void foot_contact_debounce_init(foot_contact_debounce_t *state,
                                bool initial_level);

/* Applies one raw sample and returns the current debounced level. */
bool foot_contact_debounce_update(foot_contact_debounce_t *state,
                                  bool raw_level, uint32_t threshold);

#endif /* FOOT_CONTACT_DEBOUNCE_H */
