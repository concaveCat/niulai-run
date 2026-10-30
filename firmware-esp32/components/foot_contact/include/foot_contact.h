/* Foot contact input adapter: GPIO + per-pin debounce + the installed
 * per-leg C-NC polarity conversion. No ISR: sample() is polled from the
 * control tick. */
#ifndef FOOT_CONTACT_H
#define FOOT_CONTACT_H

#include <stdbool.h>
#include <stdint.h>

#include "qw_hal.h"

/* Configures the four contact GPIOs (Kconfig pins) as inputs with internal
 * pull-ups. All four installed C-NC inputs are active-high. A pin set to -1
 * is intentionally left unbound until the
 * Carrier mapping is confirmed. */
/* Returns true only when the configured active leg input was successfully
 * configured. Other legs may remain unavailable without being read. */
bool foot_contact_init(void);

bool foot_contact_init_with_gpio(const qw_hal_gpio_t *gpio);

bool foot_contact_active_leg_ready(void);

/* Formal four-leg runtime prerequisite: every mapped contact pin configured. */
bool foot_contact_all_ready(void);

/* Polls all four pins; a debounced level only changes after
 * CONFIG_NIULAI_CONTACT_DEBOUNCE_TICKS consecutive agreeing samples.
 * Call once per control tick. */
void foot_contact_sample(void);

/* Debounced contact state of the Kconfig active leg. */
bool foot_contact_active_leg(void);

/* Debounced contact mask; bit 0..3 = FL/FR/RL/RR. Unbound inputs read clear. */
uint8_t foot_contact_mask(void);

/* Raw (undebounced) contact state using the per-leg polarity conversion;
 * leg 0..3 = FL/FR/RL/RR. */
bool foot_contact_raw(uint8_t leg);

/* Raw electrical GPIO levels; bit 0..3 = FL/FR/RL/RR, 1 means GPIO HIGH. */
uint8_t foot_contact_gpio_level_mask(void);

/* Raw (undebounced) contact mask using the per-leg polarity conversion. */
uint8_t foot_contact_raw_mask(void);

#endif /* FOOT_CONTACT_H */
