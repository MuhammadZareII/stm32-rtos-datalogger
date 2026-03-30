/**
 * @file    sensor_types.h
 * @brief   Shared data structures for all sensors and the GPS module.
 *
 * All RTOS tasks share a single global instance of SensorData_t and
 * GPS_Data_t, protected by mutexes declared in shared_data.h.
 * No task should access these structs without holding the appropriate mutex.
 */

#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdint.h>
#include "app_config.h"

/* ── Sensor identity (bus + address) ───────────────────────────────────────── */

/**
 * @brief  Identifies a sensor by its I²C bus index and 7-bit address.
 *
 * bus_idx  0 → I2C1, 1 → I2C2, 2 → I2C3, 3 → I2C4.
 * Set to INVALID_I2C_ADDR / INVALID_BUS_IDX when the slot is empty.
 */
typedef struct {
    uint8_t i2c_addr;   /**< 7-bit I²C address (unshifted)    */
    uint8_t bus_idx;    /**< Bus index (0–3)                   */
} SensorID_t;

/* ── Per-sensor data types ──────────────────────────────────────────────────── */

/** LM75 temperature sensor reading */
typedef struct {
    SensorID_t  id;
    float       temperature_c;      /**< Celsius; INVALID_FLOAT if not yet read */
    uint32_t    last_read_tick;     /**< HAL_GetTick() at last successful read   */
} LM75_t;

/** ADXL345 3-axis accelerometer reading */
typedef struct {
    SensorID_t  id;
    float       ax;                 /**< m/s²; INVALID_FLOAT if not yet read     */
    float       ay;
    float       az;
    uint32_t    last_read_tick;
} ADXL345_t;

/** SHT30 humidity sensor reading */
typedef struct {
    SensorID_t  id;
    float       humidity_pct;       /**< % RH; INVALID_FLOAT if not yet read     */
    float       temperature_c;      /**< SHT30 also provides temperature         */
    uint32_t    last_read_tick;
} SHT30_t;

/* ── GPS data ───────────────────────────────────────────────────────────────── */

/**
 * @brief  Parsed GPS fields, updated by task_gps at GPS_SAMPLE_HZ.
 *
 * Each field carries its own timestamp so the logger can flag stale data
 * independently (e.g. latitude updated but altitude still old).
 */
typedef struct {
    char     utc[16];               /**< "HH:MM:SS.sss"                         */
    char     date[16];              /**< "DD/MM/YY"                             */
    float    latitude;              /**< Decimal degrees, positive = North       */
    float    longitude;             /**< Decimal degrees, positive = East        */
    float    altitude_m;
    float    speed_kmh;
    uint8_t  fix_type;              /**< 0=no fix, 1=GPS, 2=DGPS               */
    uint8_t  satellites;

    uint32_t utc_tick;              /**< Tick of last UTC update                 */
    uint32_t position_tick;         /**< Tick of last lat/lon update             */
    uint32_t altitude_tick;
} GPS_Data_t;

/* ── Collective sensor snapshot ─────────────────────────────────────────────── */

/**
 * @brief  Complete snapshot of all sensor values at a given instant.
 *
 * The logger task serialises this struct into the binary RS232 / SD packet.
 * Access is guarded by sensor_data_mutex (see shared_data.h).
 */
typedef struct {
    LM75_t    lm75[MAX_LM75];
    uint8_t   lm75_count;

    ADXL345_t adxl[MAX_ADXL345];
    uint8_t   adxl_count;

    SHT30_t   sht30;
    uint8_t   sht30_present;

    uint8_t   failure_code;         /**< Bitmask: bit 0=LM75, 1=ADXL, 2=SHT30, 3=GPS */
} SensorData_t;

#endif /* SENSOR_TYPES_H */
