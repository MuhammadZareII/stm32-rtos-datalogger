/**
 * @file    tasks/task_logger.h
 * @brief   Data logger task — SD card writes + RS232 telemetry transmission.
 *
 * Runs at LOG_SD_PERIOD_MS (100 ms).  On every wake:
 *   1. Acquires both mutexes, copies sensor + GPS snapshots, releases mutexes.
 *   2. Builds a TelemetryPacket_t via packet_build().
 *   3. Writes the raw packet to the SD card at LOG_SD_HZ.
 *   4. Transmits the packet over RS232 at LOG_RS232_HZ (sub-sampled).
 *
 * The task also handles SD card hot-plug: if the card is removed and
 * reinserted, it re-initialises the SD writer and resumes without rebooting.
 */

#ifndef TASKS_TASK_LOGGER_H
#define TASKS_TASK_LOGGER_H

#include "FreeRTOS.h"
#include "task.h"

void task_logger(void *pvParameters);

#endif /* TASKS_TASK_LOGGER_H */
