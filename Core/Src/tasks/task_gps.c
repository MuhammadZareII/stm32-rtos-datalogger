/**
 * @file    tasks/task_gps.c
 * @brief   GPS polling task — I²C bridge read at GPS_PERIOD_MS (200 ms).
 *
 * Calls GPS_I2C_Poll() which drains the module's DDC FIFO and updates
 * globalGPSData under gps_data_mutex when a complete NMEA sentence is parsed.
 *
 * LED2 toggles on each successful position update (data-flow indicator).
 * LED3 lights on NMEA parse error and extinguishes on the next success.
 */

#include "tasks/task_gps.h"
#include "shared_data.h"
#include "drivers/gps_i2c.h"
#include "app_config.h"
#include "main.h"   /* LED pin definitions */

void task_gps(void *pvParameters)
{
    (void)pvParameters;

    GPS_I2C_Init();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(GPS_PERIOD_MS);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        if (LOCK_GPS_DATA(10))
        {
            HAL_StatusTypeDef st = GPS_I2C_Poll(&hi2c3, &globalGPSData);

            if (st == HAL_OK)
            {
                /* Toggle data-flow LED */
                HAL_GPIO_TogglePin(GPS_DATA_FLOW_LED_PORT, GPS_DATA_FLOW_LED_PIN);
                /* Clear GPS failure bit */
                if (LOCK_SENSOR_DATA(5))
                {
                    globalSensorData.failure_code &= ~(1u << 3);
                    UNLOCK_SENSOR_DATA();
                }
                HAL_GPIO_WritePin(GPS_PARSING_ERROR_LED_PORT,
                                  GPS_PARSING_ERROR_LED_PIN, GPIO_PIN_RESET);
            }
            else
            {
                HAL_GPIO_WritePin(GPS_PARSING_ERROR_LED_PORT,
                                  GPS_PARSING_ERROR_LED_PIN, GPIO_PIN_SET);
                if (LOCK_SENSOR_DATA(5))
                {
                    globalSensorData.failure_code |= (1u << 3);
                    UNLOCK_SENSOR_DATA();
                }
            }

            UNLOCK_GPS_DATA();
        }
    }
}
