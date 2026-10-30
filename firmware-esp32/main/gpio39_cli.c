#include "gpio39_cli.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define QW_GPIO39_BUTTON GPIO_NUM_39
#define QW_GPIO39_POLL_MS 10

void qw_gpio39_cli_run(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << QW_GPIO39_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    for (;;) {
        /* C-NC wiring: open = pulled HIGH = pressed (1), closed = LOW =
         * released (0). Emit the current state on every poll. */
        int state = gpio_get_level(QW_GPIO39_BUTTON) ? 1 : 0;
        printf("%d\n", state);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(QW_GPIO39_POLL_MS));
    }
}
