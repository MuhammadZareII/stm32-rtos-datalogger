/**
 * @file    tasks/task_temp.c
 * @brief   Sensor-A temperature + Sensor-C humidity read task — 10 Hz.
 *
 * Iterates over all discovered Sensor-A sensors on all buses, then reads the
 * single Sensor-C.  Period: SENSOR_A_PERIOD_MS (100 ms).
 *
 * The Sensor-C single-shot measurement requires ~15 ms between trigger and
 * read.  This is handled by triggering at the start of the task body and
 * reading at the end, using a short vTaskDelay in between so the CPU is
 * yielded during the measurement window.
 */

#include "tasks/task_temp.h"
#include "shared_data.h"
#include "drivers/sensor_a.h"
#include "drivers/sensor_c.h"
#include "app_config.h"

static I2C_HandleTypeDef * const i2c_buses[MAX_I2C_BUSES] = {
    &hi2c1, &hi2c2, &hi2c3, &hi2c4
};

void task_temp(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_A_PERIOD_MS);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* ── Sensor-A: read all discovered sensors ──────────────────────────── */
        if (LOCK_SENSOR_DATA(10))
        {
            for (uint8_t i = 0; i < globalSensorData.sensor_a_count; i++)
            {
                SensorA_t *s   = &globalSensorData.sensor_a[i];
                uint8_t bus = s->id.bus_idx;
                uint8_t adr = s->id.i2c_addr;

                if (bus >= MAX_I2C_BUSES || adr == INVALID_I2C_ADDR)
                    continue;

                float temp;
                if (SensorA_ReadTemp(i2c_buses[bus], adr, &temp) == HAL_OK)
                {
                    s->temperature_c    = temp;
                    s->last_read_tick   = HAL_GetTick();
                    globalSensorData.failure_code &= ~(1u << 0);
                }
                else
                {
                    s->temperature_c = INVALID_FLOAT;
                    globalSensorData.failure_code |= (1u << 0);
                }
            }
            UNLOCK_SENSOR_DATA();
        }

        /* ── Sensor-C: trigger measurement, yield 20 ms, read result ───────── */
        if (LOCK_SENSOR_DATA(10))
        {
            uint8_t present = globalSensorData.sensor_c_present;
            uint8_t bus     = globalSensorData.sensor_c.id.bus_idx;
            uint8_t adr     = globalSensorData.sensor_c.id.i2c_addr;
            UNLOCK_SENSOR_DATA();

            if (present && bus < MAX_I2C_BUSES)
            {
                float hum, tmp;
                /* SensorC_ReadBoth triggers + waits internally */
                if (SensorC_ReadBoth(i2c_buses[bus], adr, &hum, &tmp) == HAL_OK)
                {
                    if (LOCK_SENSOR_DATA(10))
                    {
                        globalSensorData.sensor_c.humidity_pct   = hum;
                        globalSensorData.sensor_c.temperature_c  = tmp;
                        globalSensorData.sensor_c.last_read_tick = HAL_GetTick();
                        globalSensorData.failure_code &= ~(1u << 2);
                        UNLOCK_SENSOR_DATA();
                    }
                }
                else
                {
                    if (LOCK_SENSOR_DATA(10))
                    {
                        globalSensorData.sensor_c.humidity_pct  = INVALID_FLOAT;
                        globalSensorData.failure_code |= (1u << 2);
                        UNLOCK_SENSOR_DATA();
                    }
                }
            }
        }
    }
}
