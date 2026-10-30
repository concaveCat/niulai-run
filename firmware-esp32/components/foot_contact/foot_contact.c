#include "sdkconfig.h"

#include "foot_contact.h"
#include "foot_contact_debounce.h"

#include "qw_hal_espidf.h"

typedef struct {
    int pin;
    bool contact_active_high;
    foot_contact_debounce_t debounce;
} qw_contact_pin_t;

static qw_contact_pin_t s_pins[4];
static bool s_initialized;
static uint8_t s_valid_mask;
static qw_hal_gpio_t s_gpio;

static int active_leg_index(void)
{
#if defined(CONFIG_NIULAI_ACTIVE_LEG_FL)
    return 0;
#elif defined(CONFIG_NIULAI_ACTIVE_LEG_FR)
    return 1;
#elif defined(CONFIG_NIULAI_ACTIVE_LEG_RL)
    return 2;
#else
    return 3; /* RR */
#endif
}

static bool raw_pressed(const qw_contact_pin_t *pin)
{
    const int level = s_gpio.read(s_gpio.ctx, pin->pin);

    if (level != 0 && level != 1) {
        return false;
    }
    /* All four installed C-NC inputs use the same active-high semantic after
     * the FL correction. */
    return pin->contact_active_high ? level == 1 : level == 0;
}

bool foot_contact_init_with_gpio(const qw_hal_gpio_t *gpio)
{
    const int pins[4] = {
        CONFIG_NIULAI_CONTACT_PIN_FL,
        CONFIG_NIULAI_CONTACT_PIN_FR,
        CONFIG_NIULAI_CONTACT_PIN_RL,
        CONFIG_NIULAI_CONTACT_PIN_RR,
    };
    s_gpio = gpio == NULL ? qw_hal_espidf_gpio() : *gpio;
    s_initialized = false;
    s_valid_mask = 0U;
    for (int i = 0; i < 4; i++) {
        s_pins[i].pin = pins[i] >= 0 ? pins[i] : -1;
        s_pins[i].contact_active_high = true;
        foot_contact_debounce_init(&s_pins[i].debounce, false);
        if (s_pins[i].pin >= 0 && s_gpio.configure_input != NULL &&
            s_gpio.read != NULL &&
            s_gpio.configure_input(s_gpio.ctx, s_pins[i].pin, true) ==
                QW_HAL_OK) {
            s_valid_mask |= (uint8_t)(1U << i);
        } else {
            /* A failed configure must never be sampled as if it were a live
             * GPIO. Keep the configured pin only for diagnostics. */
            s_pins[i].pin = -1;
        }
    }
    s_initialized = s_valid_mask != 0U;
    return (s_valid_mask & (uint8_t)(1U << active_leg_index())) != 0U;
}

bool foot_contact_init(void)
{
    return foot_contact_init_with_gpio(NULL);
}

void foot_contact_sample(void)
{
    const int needed = CONFIG_NIULAI_CONTACT_DEBOUNCE_TICKS;

    if (!s_initialized) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        if ((s_valid_mask & (uint8_t)(1U << i)) == 0U) {
            continue;
        }
        bool raw = raw_pressed(&s_pins[i]);
        (void)foot_contact_debounce_update(&s_pins[i].debounce, raw, needed);
    }
}

bool foot_contact_active_leg(void)
{
    return s_initialized ? s_pins[active_leg_index()].debounce.debounced : false;
}

bool foot_contact_active_leg_ready(void)
{
    return (s_valid_mask & (uint8_t)(1U << active_leg_index())) != 0U;
}

bool foot_contact_all_ready(void)
{
    return s_valid_mask == 0x0FU;
}

uint8_t foot_contact_mask(void)
{
    uint8_t mask = 0;

    if (!s_initialized) {
        return 0;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if ((s_valid_mask & (uint8_t)(1U << i)) != 0U &&
            s_pins[i].debounce.debounced) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}

bool foot_contact_raw(uint8_t leg)
{
    return leg < 4 && (s_valid_mask & (uint8_t)(1U << leg)) != 0U
               ? raw_pressed(&s_pins[leg])
               : false;
}

uint8_t foot_contact_gpio_level_mask(void)
{
    uint8_t mask = 0;

    if (!s_initialized) {
        return 0;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if ((s_valid_mask & (uint8_t)(1U << i)) != 0U &&
            s_gpio.read(s_gpio.ctx, s_pins[i].pin) == 1) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}

uint8_t foot_contact_raw_mask(void)
{
    uint8_t mask = 0;

    if (!s_initialized) {
        return 0;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if ((s_valid_mask & (uint8_t)(1U << i)) != 0U &&
            raw_pressed(&s_pins[i])) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}
