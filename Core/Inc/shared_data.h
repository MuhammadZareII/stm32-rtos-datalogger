/**
 * @file    shared_data.h
 * @brief   Global data instances and RTOS synchronisation handles.
 *
 * All inter-task shared state lives here.  Every task that reads or writes
 * sensor data must acquire the corresponding mutex first.
 *
 * Locking discipline
 * ------------------
 *   sensor_data_mutex  — guards globalSensorData
 *   gps_data_mutex     — guards globalGPSData
 *
 * Mutexes are created in main.c before the scheduler starts.
 * Never hold both mutexes simultaneously to prevent priority inversion.
 *
 * Usage pattern (FreeRTOS):
 * @code
 *   if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
 *       globalSensorData.lm75[0].temperature_c = reading;
 *       xSemaphoreGive(sensor_data_mutex);
 *   }
 * @endcode
 */

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "sensor_types.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* ── Global data instances (defined in shared_data.c) ──────────────────────── */
extern SensorData_t  globalSensorData;
extern GPS_Data_t    globalGPSData;

/* ── Mutex handles (created in main.c before scheduler start) ──────────────── */
extern SemaphoreHandle_t sensor_data_mutex;
extern SemaphoreHandle_t gps_data_mutex;

/* ── Convenience macros ─────────────────────────────────────────────────────── */

/** Acquire sensor_data_mutex; returns false and logs if timeout fires. */
#define LOCK_SENSOR_DATA(timeout_ms) \
    (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)

#define UNLOCK_SENSOR_DATA() \
    xSemaphoreGive(sensor_data_mutex)

#define LOCK_GPS_DATA(timeout_ms) \
    (xSemaphoreTake(gps_data_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)

#define UNLOCK_GPS_DATA() \
    xSemaphoreGive(gps_data_mutex)

#endif /* SHARED_DATA_H */
