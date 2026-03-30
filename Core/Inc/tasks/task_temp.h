/**
 * @file    tasks/task_temp.h
 * @brief   Sensor-A temperature + Sensor-C humidity task — 10 Hz periodic read.
 *
 * Iterates over all discovered Sensor-A sensors and the single Sensor-C.
 * Uses vTaskDelayUntil at SENSOR_A_PERIOD_MS (100 ms).
 */

#ifndef TASKS_TASK_TEMP_H
#define TASKS_TASK_TEMP_H

#include "FreeRTOS.h"
#include "task.h"

void task_temp(void *pvParameters);

#endif /* TASKS_TASK_TEMP_H */
