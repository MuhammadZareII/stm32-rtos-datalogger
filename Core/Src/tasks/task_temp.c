/**
 * @file    tasks/task_temp.c
 * @brief   LM75 temperature + SHT30 humidity read task — 10 Hz.
 *
 * Iterates over all discovered LM75 sensors on all buses, then reads the
 * single SHT30.  Period: LM75_PERIOD_MS (100 ms).
 *
 * The SHT30 single-shot measurement requires ~15 ms between trigger and
 * read.  This is handled by triggering at the start of the task body and
 * reading at the end, using a short vTaskDelay in between so the CPU is
 * yielded during the measurement window.
 */

#include "tasks/task_temp.h"
#include "shared_data.h"
#include "drivers/lm75.h"
#include "drivers/sht30.h"
#include "app_config.h"

static I2C_HandleTypeDef * const i2c_buses[MAX_I2C_BUSES] = {
    &hi2c1, &hi2c2, &hi2c3, &hi2c4
};

void task_temp(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(LM75_PERIOD_MS);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* ── LM75: read all discovered sensors ──────────────────────────── */
        if (LOCK_SENSOR_DATA(10))
        {
            for (uint8_t i = 0; i < globalSensorData.lm75_count; i++)
            {
                LM75_t *s   = &globalSensorData.lm75[i];
                uint8_t bus = s->id.bus_idx;
                uint8_t adr = s->id.i2c_addr;

                if (bus >= MAX_I2C_BUSES || adr == INVALID_I2C_ADDR)
                    continue;

                float temp;
                if (LM75_ReadTemp(i2c_buses[bus], adr, &temp) == HAL_OK)
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

        /* ── SHT30: trigger measurement, yield 20 ms, read result ───────── */
        if (LOCK_SENSOR_DATA(10))
        {
            uint8_t present = globalSensorData.sht30_present;
            uint8_t bus     = globalSensorData.sht30.id.bus_idx;
            uint8_t adr     = globalSensorData.sht30.id.i2c_addr;
            UNLOCK_SENSOR_DATA();

            if (present && bus < MAX_I2C_BUSES)
            {
                float hum, tmp;
                /* SHT30_ReadBoth triggers + waits internally */
                if (SHT30_ReadBoth(i2c_buses[bus], adr, &hum, &tmp) == HAL_OK)
                {
                    if (LOCK_SENSOR_DATA(10))
                    {
                        globalSensorData.sht30.humidity_pct   = hum;
                        globalSensorData.sht30.temperature_c  = tmp;
                        globalSensorData.sht30.last_read_tick = HAL_GetTick();
                        globalSensorData.failure_code &= ~(1u << 2);
                        UNLOCK_SENSOR_DATA();
                    }
                }
                else
                {
                    if (LOCK_SENSOR_DATA(10))
                    {
                        globalSensorData.sht30.humidity_pct  = INVALID_FLOAT;
                        globalSensorData.failure_code |= (1u << 2);
                        UNLOCK_SENSOR_DATA();
                    }
                }
            }
        }
    }
}
