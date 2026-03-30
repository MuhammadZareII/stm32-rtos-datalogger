/**
 * @file    tasks/task_temp.h
 * @brief   LM75 temperature + SHT30 humidity task — 10 Hz periodic read.
 *
 * Iterates over all discovered LM75 sensors and the single SHT30.
 * Uses vTaskDelayUntil at LM75_PERIOD_MS (100 ms).
 */

#ifndef TASKS_TASK_TEMP_H
#define TASKS_TASK_TEMP_H

#include "FreeRTOS.h"
#include "task.h"

void task_temp(void *pvParameters);

#endif /* TASKS_TASK_TEMP_H */
