/**
 * @file    tasks/task_scan.h
 * @brief   I²C hot-plug scan task — background sensor discovery.
 *
 * Scans all four I²C buses for Sensor-A, Sensor-B, and Sensor-C address ranges
 * every SCAN_PERIOD_MS (2000 ms).  Newly found devices are added to
 * globalSensorData; devices that no longer ACK are marked absent.
 *
 * Runs at the lowest non-idle priority so it never delays sensor reads.
 * LED4 blinks briefly on each completed scan.
 */

#ifndef TASKS_TASK_SCAN_H
#define TASKS_TASK_SCAN_H

#include "FreeRTOS.h"
#include "task.h"

void task_scan(void *pvParameters);

#endif /* TASKS_TASK_SCAN_H */
