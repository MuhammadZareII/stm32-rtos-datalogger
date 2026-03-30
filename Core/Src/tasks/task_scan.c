/**
 * @file    tasks/task_scan.c
 * @brief   I²C hot-plug sensor scan task — background, 0.5 Hz.
 *
 * Scans all four buses for the known sensor address ranges:
 *   Sensor-A:   0x48–0x4F  (8 possible addresses per bus)
 *   Sensor-B: 0x53, 0x1D (2 addresses per bus)
 *   Sensor-C:  0x44, 0x45 (I2C3 only in this design)
 *
 * Newly discovered devices are appended to globalSensorData; devices that
 * no longer ACK are removed.  The scan task also initialises newly found
 * Sensor-B units (writes DATA_FORMAT and POWER_CTL registers).
 *
 * Priority is deliberately the lowest non-idle priority: a 2-second scan
 * can be interrupted at any time by sensor read tasks without affecting
 * timing accuracy.
 */

#include "tasks/task_scan.h"
#include "shared_data.h"
#include "drivers/sensor_a.h"
#include "drivers/sensor_b.h"
#include "drivers/sensor_c.h"
#include "app_config.h"
#include "main.h"   /* LED definitions */

static I2C_HandleTypeDef * const i2c_buses[MAX_I2C_BUSES] = {
    &hi2c1, &hi2c2, &hi2c3, &hi2c4
};

/* Sensor-A probes 0x48..0x4F on every bus */
static const uint8_t sensor_a_addrs[] = {
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
};

/* Sensor-B addresses */
static const uint8_t sensor_b_addrs[] = {
    SENSOR_B_ADDR_0, SENSOR_B_ADDR_1
};

/* ── Helper: find a sensor slot by bus + address ────────────────────────── */
static int8_t find_sensor_a_slot(uint8_t bus, uint8_t addr)
{
    for (uint8_t i = 0; i < globalSensorData.sensor_a_count; i++)
        if (globalSensorData.sensor_a[i].id.bus_idx  == bus &&
            globalSensorData.sensor_a[i].id.i2c_addr == addr)
            return (int8_t)i;
    return -1;
}

static int8_t find_sensor_b_slot(uint8_t bus, uint8_t addr)
{
    for (uint8_t i = 0; i < globalSensorData.sensor_b_count; i++)
        if (globalSensorData.sensor_b[i].id.bus_idx  == bus &&
            globalSensorData.sensor_b[i].id.i2c_addr == addr)
            return (int8_t)i;
    return -1;
}

void task_scan(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SCAN_PERIOD_MS);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        HAL_GPIO_TogglePin(SENSOR_SCAN_LED_PORT, SENSOR_SCAN_LED_PIN);

        if (!LOCK_SENSOR_DATA(50))
            continue;

        /* ── Sensor-A scan ───────────────────────────────────────────────────── */
        for (uint8_t bus = 0; bus < MAX_I2C_BUSES; bus++)
        {
            for (uint8_t ai = 0; ai < sizeof(sensor_a_addrs); ai++)
            {
                uint8_t addr = sensor_a_addrs[ai];
                uint8_t present = (SensorA_Probe(i2c_buses[bus], addr) == HAL_OK);

                int8_t slot = find_sensor_a_slot(bus, addr);

                if (present && slot < 0 &&
                    globalSensorData.sensor_a_count < MAX_SENSOR_A)
                {
                    /* New device — add to array */
                    uint8_t n = globalSensorData.sensor_a_count;
                    globalSensorData.sensor_a[n].id.bus_idx  = bus;
                    globalSensorData.sensor_a[n].id.i2c_addr = addr;
                    globalSensorData.sensor_a[n].temperature_c = INVALID_FLOAT;
                    globalSensorData.sensor_a[n].last_read_tick = 0;
                    globalSensorData.sensor_a_count++;
                }
                else if (!present && slot >= 0)
                {
                    /* Device lost — remove by swapping with last slot */
                    uint8_t last = globalSensorData.sensor_a_count - 1;
                    globalSensorData.sensor_a[slot] = globalSensorData.sensor_a[last];
                    globalSensorData.sensor_a_count--;
                }
            }
        }

        /* ── Sensor-B scan ────────────────────────────────────────────────── */
        for (uint8_t bus = 0; bus < MAX_I2C_BUSES; bus++)
        {
            for (uint8_t ai = 0; ai < sizeof(sensor_b_addrs); ai++)
            {
                uint8_t addr = sensor_b_addrs[ai];
                uint8_t present = (SensorB_Probe(i2c_buses[bus], addr) == HAL_OK);

                int8_t slot = find_sensor_b_slot(bus, addr);

                if (present && slot < 0 &&
                    globalSensorData.sensor_b_count < MAX_SENSOR_B)
                {
                    /* Initialise the newly found Sensor-B */
                    SensorB_Init(i2c_buses[bus], addr);

                    uint8_t n = globalSensorData.sensor_b_count;
                    globalSensorData.sensor_b[n].id.bus_idx  = bus;
                    globalSensorData.sensor_b[n].id.i2c_addr = addr;
                    globalSensorData.sensor_b[n].ax = INVALID_FLOAT;
                    globalSensorData.sensor_b[n].ay = INVALID_FLOAT;
                    globalSensorData.sensor_b[n].az = INVALID_FLOAT;
                    globalSensorData.sensor_b[n].last_read_tick = 0;
                    globalSensorData.sensor_b_count++;
                }
                else if (!present && slot >= 0)
                {
                    uint8_t last = globalSensorData.sensor_b_count - 1;
                    globalSensorData.sensor_b[slot] = globalSensorData.sensor_b[last];
                    globalSensorData.sensor_b_count--;
                }
            }
        }

        /* ── Sensor-C scan (I2C3 only) ──────────────────────────────────────── */
        {
            uint8_t addr = SENSOR_C_ADDR_DEFAULT;
            uint8_t present = (SensorC_Probe(&hi2c3, addr) == HAL_OK);
            if (present && !globalSensorData.sensor_c_present)
            {
                globalSensorData.sensor_c.id.bus_idx  = 2;   /* I2C3 */
                globalSensorData.sensor_c.id.i2c_addr = addr;
                globalSensorData.sensor_c.humidity_pct  = INVALID_FLOAT;
                globalSensorData.sensor_c.temperature_c = INVALID_FLOAT;
                globalSensorData.sensor_c_present = 1;
            }
            else if (!present)
            {
                globalSensorData.sensor_c_present = 0;
            }
        }

        UNLOCK_SENSOR_DATA();
    }
}
