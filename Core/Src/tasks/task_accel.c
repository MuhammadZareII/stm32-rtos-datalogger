/**
 * @file    tasks/task_accel.c
 * @brief   Sensor-B accelerometer read task — 100 Hz, highest priority.
 *
 * Uses vTaskDelayUntil() to achieve a jitter-free 10 ms period regardless
 * of the I²C transaction duration.  Each iteration reads all discovered
 * Sensor-B units in sequence under the sensor_data_mutex.
 *
 * Timing analysis:
 *   I²C @ 400 kHz, 6 data bytes + address + register: ~200 µs per sensor.
 *   4 sensors: ~800 µs read time.  With 10 ms period, CPU load ≈ 8%.
 *   Mutex hold time is bounded by the read loop — release before blocking.
 */

#include "tasks/task_accel.h"
#include "shared_data.h"
#include "drivers/sensor_b.h"
#include "app_config.h"

/* Bus handle array — index matches SensorID_t.bus_idx */
static I2C_HandleTypeDef * const i2c_buses[MAX_I2C_BUSES] = {
    &hi2c1, &hi2c2, &hi2c3, &hi2c4
};

void task_accel(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SENSOR_B_PERIOD_MS);

    for (;;)
    {
        /* ── Wait until next period ──────────────────────────────────────── */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* ── Read all discovered Sensor-B sensors ─────────────────────────── */
        if (!LOCK_SENSOR_DATA(5))   /* 5 ms timeout — skip if contended */
            continue;

        for (uint8_t i = 0; i < globalSensorData.sensor_b_count; i++)
        {
            SensorB_t *s   = &globalSensorData.sensor_b[i];
            uint8_t    bus = s->id.bus_idx;
            uint8_t    adr = s->id.i2c_addr;

            if (bus >= MAX_I2C_BUSES || adr == INVALID_I2C_ADDR)
                continue;

            float ax, ay, az;
            if (SensorB_ReadAccel(i2c_buses[bus], adr, &ax, &ay, &az) == HAL_OK)
            {
                s->ax = ax;
                s->ay = ay;
                s->az = az;
                s->last_read_tick = HAL_GetTick();
                /* Clear Sensor-B failure bit */
                globalSensorData.failure_code &= ~(1u << 1);
            }
            else
            {
                s->ax = s->ay = s->az = INVALID_FLOAT;
                globalSensorData.failure_code |= (1u << 1);
            }
        }

        UNLOCK_SENSOR_DATA();
    }
}
