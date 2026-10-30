/* 100 Hz control task: foot contact sampling -> FSM tick -> command
 * dispatch through the safety gate. The FSM is single-writer (this task);
 * BLE input is read through the spinlock-guarded snapshot. */
#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include <stdbool.h>

#include "motion_strategy.h"

/* Creates the task/UART synchronization object before either can run. */
void control_task_sync_init(void);
void control_task(void *arg);

/* Copies one consistent FSM snapshot for UART telemetry. */
bool control_fsm_snapshot(qw_motion_snapshot_t *snapshot);

/* UART "reset": leg_fsm_reset + log. */
void control_fsm_reset(void);

#endif /* CONTROL_TASK_H */
