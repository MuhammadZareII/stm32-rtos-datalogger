/**
 * @file    tasks/task_accel.h
 * @brief   Sensor-B accelerometer task — highest-priority periodic read.
 *
 * Runs at SENSOR_B_PERIOD_MS (10 ms = 100 Hz) using vTaskDelayUntil for
 * jitter-free timing.  Writes results to globalSensorData under
 * sensor_data_mutex.
 */

#ifndef TASKS_TASK_ACCEL_H
#define TASKS_TASK_ACCEL_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief  FreeRTOS task entry point for accelerometer reads.
 *
 * Create with:
 * @code
 *   xTaskCreate(task_accel, "accel", STACK_ACCEL, NULL, TASK_PRIO_ACCEL, NULL);
 * @endcode
 */
void task_accel(void *pvParameters);

#endif /* TASKS_TASK_ACCEL_H */
