#include "sdkconfig.h"

#include "foot_contact_test.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_timer.h"
#include "foot_contact.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define QW_KW10_TEST_PERIOD_MS (1000 / CONFIG_NIULAI_CONTROL_TICK_HZ)

static void log_sample(uint32_t sample, uint8_t gpio_level,
                       uint8_t raw_contact, uint8_t debounced)
{
    printf("KW10_SAMPLE n=%lu t_ms=%lld gpio=0x%02X raw_contact=0x%02X "
           "debounced=0x%02X\n",
           (unsigned long)sample,
           (long long)(esp_timer_get_time() / 1000),
           (unsigned)gpio_level, (unsigned)raw_contact, (unsigned)debounced);
}

void qw_foot_contact_test_run(void)
{
    const TickType_t period = pdMS_TO_TICKS(QW_KW10_TEST_PERIOD_MS);
    TickType_t wake = xTaskGetTickCount();
    uint32_t sample = 0;
    uint8_t last_gpio = 0xFF;
    uint8_t last_raw = 0xFF;
    uint8_t last_debounced = 0xFF;

    printf("KW10_TEST pins FL=%d FR=%d RL=%d RR=%d "
           "gpio39=FR gpio40=FL gpio41=RR gpio42=RL tick_hz=%d "
           "debounce_ticks=%d polarity=FL:active-low,FR:active-high,"
           "RL:active-high,RR:active-high\n",
           CONFIG_NIULAI_CONTACT_PIN_FL,
           CONFIG_NIULAI_CONTACT_PIN_FR,
           CONFIG_NIULAI_CONTACT_PIN_RL,
           CONFIG_NIULAI_CONTACT_PIN_RR,
           CONFIG_NIULAI_CONTROL_TICK_HZ,
           CONFIG_NIULAI_CONTACT_DEBOUNCE_TICKS);
    printf("KW10_TEST masks: bit0=FL bit1=FR bit2=RL bit3=RR; "
           "gpio=physical level, raw_contact=per-leg polarity, "
           "debounced=filtered\n");

    foot_contact_init();
    for (;;) {
        uint8_t gpio_level = foot_contact_gpio_level_mask();
        uint8_t raw_contact = foot_contact_raw_mask();

        foot_contact_sample();
        uint8_t debounced = foot_contact_mask();
        sample++;

        if (gpio_level != last_gpio || raw_contact != last_raw ||
            debounced != last_debounced) {
            log_sample(sample, gpio_level, raw_contact, debounced);
            last_gpio = gpio_level;
            last_raw = raw_contact;
            last_debounced = debounced;
        } else if (sample % CONFIG_NIULAI_CONTROL_TICK_HZ == 0) {
            log_sample(sample, gpio_level, raw_contact, debounced);
        }

        vTaskDelayUntil(&wake, period);
    }
}
