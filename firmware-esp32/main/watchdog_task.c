#include "sdkconfig.h"

#include "watchdog_task.h"

#include "ble_input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qw_hal_espidf.h"
#include "safety_gate.h"
#include "uart_log.h"

#define QW_WATCHDOG_TICK_PERIOD_MS 100

void watchdog_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    qw_hal_clock_t clock = qw_hal_espidf_clock();

    for (;;) {
        int64_t now_us = clock.now_us(clock.ctx);

        /* Failsafe check first: on a fresh trip, the mask is now zero. */
        if (ble_input_watchdog_check(now_us)) {
            qw_uart_log_watchdog(ble_input_snapshot().last_rx_time_us);
            ble_input_notify_status();
            safety_gate_on_failsafe();
        }

        /* Acceptance monitor feed: fresh = a valid packet arrived within
         * the watchdog interval (inter-arrival bound for the gate window). */
        remote_leg_input_t snap = ble_input_snapshot();
        int64_t timeout_us = (int64_t)CONFIG_NIULAI_WATCHDOG_TIMEOUT_MS *
                             1000;
        bool fresh = qw_remote_input_is_fresh(&snap, now_us, timeout_us);
        safety_gate_acceptance_feed(fresh, snap.failsafe_active);

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(QW_WATCHDOG_TICK_PERIOD_MS));
    }
}
