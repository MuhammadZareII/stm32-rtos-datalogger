/**
 * @file    shared_data.c
 * @brief   Global data instance definitions and mutex handle storage.
 *
 * Definitions only — mutexes are created (not just declared) in main.c
 * before the scheduler starts, so they are valid by the time any task runs.
 */

#include "shared_data.h"
#include <string.h>

SensorData_t  globalSensorData;
GPS_Data_t    globalGPSData;

SemaphoreHandle_t sensor_data_mutex = NULL;
SemaphoreHandle_t gps_data_mutex    = NULL;
