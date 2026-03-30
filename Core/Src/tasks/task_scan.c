/**
 * @file    tasks/task_scan.c
 * @brief   I²C hot-plug sensor scan task — background, 0.5 Hz.
 *
 * Scans all four buses for the known sensor address ranges:
 *   LM75:   0x48–0x4F  (8 possible addresses per bus)
 *   ADXL345: 0x53, 0x1D (2 addresses per bus)
 *   SHT30:  0x44, 0x45 (I2C3 only in this design)
 *
 * Newly discovered devices are appended to globalSensorData; devices that
 * no longer ACK are removed.  The scan task also initialises newly found
 * ADXL345 units (writes DATA_FORMAT and POWER_CTL registers).
 *
 * Priority is deliberately the lowest non-idle priority: a 2-second scan
 * can be interrupted at any time by sensor read tasks without affecting
 * timing accuracy.
 */

#include "tasks/task_scan.h"
#include "shared_data.h"
#include "drivers/lm75.h"
#include "drivers/adxl345.h"
#include "drivers/sht30.h"
#include "app_config.h"
#include "main.h"   /* LED definitions */

static I2C_HandleTypeDef * const i2c_buses[MAX_I2C_BUSES] = {
    &hi2c1, &hi2c2, &hi2c3, &hi2c4
};

/* LM75 probes 0x48..0x4F on every bus */
static const uint8_t lm75_addrs[] = {
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
};

/* ADXL345 addresses */
static const uint8_t adxl_addrs[] = {
    ADXL345_ADDR_LOW, ADXL345_ADDR_HIGH
};

/* ── Helper: find a sensor slot by bus + address ────────────────────────── */
static int8_t find_lm75_slot(uint8_t bus, uint8_t addr)
{
    for (uint8_t i = 0; i < globalSensorData.lm75_count; i++)
        if (globalSensorData.lm75[i].id.bus_idx  == bus &&
            globalSensorData.lm75[i].id.i2c_addr == addr)
            return (int8_t)i;
    return -1;
}

static int8_t find_adxl_slot(uint8_t bus, uint8_t addr)
{
    for (uint8_t i = 0; i < globalSensorData.adxl_count; i++)
        if (globalSensorData.adxl[i].id.bus_idx  == bus &&
            globalSensorData.adxl[i].id.i2c_addr == addr)
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

        /* ── LM75 scan ───────────────────────────────────────────────────── */
        for (uint8_t bus = 0; bus < MAX_I2C_BUSES; bus++)
        {
            for (uint8_t ai = 0; ai < sizeof(lm75_addrs); ai++)
            {
                uint8_t addr = lm75_addrs[ai];
                uint8_t present = (LM75_Probe(i2c_buses[bus], addr) == HAL_OK);

                int8_t slot = find_lm75_slot(bus, addr);

                if (present && slot < 0 &&
                    globalSensorData.lm75_count < MAX_LM75)
                {
                    /* New device — add to array */
                    uint8_t n = globalSensorData.lm75_count;
                    globalSensorData.lm75[n].id.bus_idx  = bus;
                    globalSensorData.lm75[n].id.i2c_addr = addr;
                    globalSensorData.lm75[n].temperature_c = INVALID_FLOAT;
                    globalSensorData.lm75[n].last_read_tick = 0;
                    globalSensorData.lm75_count++;
                }
                else if (!present && slot >= 0)
                {
                    /* Device lost — remove by swapping with last slot */
                    uint8_t last = globalSensorData.lm75_count - 1;
                    globalSensorData.lm75[slot] = globalSensorData.lm75[last];
                    globalSensorData.lm75_count--;
                }
            }
        }

        /* ── ADXL345 scan ────────────────────────────────────────────────── */
        for (uint8_t bus = 0; bus < MAX_I2C_BUSES; bus++)
        {
            for (uint8_t ai = 0; ai < sizeof(adxl_addrs); ai++)
            {
                uint8_t addr = adxl_addrs[ai];
                uint8_t present = (ADXL345_Probe(i2c_buses[bus], addr) == HAL_OK);

                int8_t slot = find_adxl_slot(bus, addr);

                if (present && slot < 0 &&
                    globalSensorData.adxl_count < MAX_ADXL345)
                {
                    /* Initialise the newly found ADXL345 */
                    ADXL345_Init(i2c_buses[bus], addr);

                    uint8_t n = globalSensorData.adxl_count;
                    globalSensorData.adxl[n].id.bus_idx  = bus;
                    globalSensorData.adxl[n].id.i2c_addr = addr;
                    globalSensorData.adxl[n].ax = INVALID_FLOAT;
                    globalSensorData.adxl[n].ay = INVALID_FLOAT;
                    globalSensorData.adxl[n].az = INVALID_FLOAT;
                    globalSensorData.adxl[n].last_read_tick = 0;
                    globalSensorData.adxl_count++;
                }
                else if (!present && slot >= 0)
                {
                    uint8_t last = globalSensorData.adxl_count - 1;
                    globalSensorData.adxl[slot] = globalSensorData.adxl[last];
                    globalSensorData.adxl_count--;
                }
            }
        }

        /* ── SHT30 scan (I2C3 only) ──────────────────────────────────────── */
        {
            uint8_t addr = SHT30_ADDR_DEFAULT;
            uint8_t present = (SHT30_Probe(&hi2c3, addr) == HAL_OK);
            if (present && !globalSensorData.sht30_present)
            {
                globalSensorData.sht30.id.bus_idx  = 2;   /* I2C3 */
                globalSensorData.sht30.id.i2c_addr = addr;
                globalSensorData.sht30.humidity_pct  = INVALID_FLOAT;
                globalSensorData.sht30.temperature_c = INVALID_FLOAT;
                globalSensorData.sht30_present = 1;
            }
            else if (!present)
            {
                globalSensorData.sht30_present = 0;
            }
        }

        UNLOCK_SENSOR_DATA();
    }
}
