/**
 * @file    tasks/task_gps.h
 * @brief   GPS polling task — reads I²C bridge at GPS_PERIOD_MS (200 ms).
 *
 * Updates globalGPSData under gps_data_mutex.
 * LED2 blinks on each successful NMEA update; LED3 lights on parse error.
 */

#ifndef TASKS_TASK_GPS_H
#define TASKS_TASK_GPS_H

#include "FreeRTOS.h"
#include "task.h"

void task_gps(void *pvParameters);

#endif /* TASKS_TASK_GPS_H */
