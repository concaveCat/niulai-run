/* Watchdog task: 100 ms loop, 4096-byte xTaskCreate stack depth (a previous
 * team build overflowed at 2048 bytes - do not shrink without evidence).
 *
 * Each tick: ble_input_watchdog_check against esp_timer time; on a failsafe
 * trip -> notify STATUS + WATCHDOG log + safety_gate_on_failsafe. Then feeds
 * the actuation acceptance monitor with whether a fresh packet arrived
 * within the watchdog interval. */
#ifndef WATCHDOG_TASK_H
#define WATCHDOG_TASK_H

void watchdog_task(void *arg);

#endif /* WATCHDOG_TASK_H */
