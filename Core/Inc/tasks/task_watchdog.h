/**
 * @file    tasks/task_watchdog.h
 * @brief   IWDG watchdog feed task.
 *
 * Reloads the Independent Watchdog every WATCHDOG_FEED_PERIOD_MS.
 * This task must never be starved — assign it TASK_PRIO_WATCHDOG which is
 * higher than the scan task but lower than sensor tasks.
 *
 * If the system deadlocks (e.g. a mutex is held permanently), the IWDG
 * will fire after ~800 ms and reset the MCU.
 */

#ifndef TASKS_TASK_WATCHDOG_H
#define TASKS_TASK_WATCHDOG_H

#include "FreeRTOS.h"
#include "task.h"

void task_watchdog(void *pvParameters);

#endif /* TASKS_TASK_WATCHDOG_H */
