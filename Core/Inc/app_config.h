/**
 * @file    app_config.h
 * @brief   Central application configuration — all tunable constants in one place.
 *
 * Edit this file to adjust sample rates, bus assignments, sensor counts,
 * packet layout constants, and task stack / priority settings without
 * touching driver or task source files.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ── Target MCU ────────────────────────────────────────────────────────────── */
#include "stm32h7xx_hal.h"

/* ── I2C bus handles (defined in main.c, declared extern elsewhere) ────────── */
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;
extern I2C_HandleTypeDef hi2c4;

/* ── UART handles ──────────────────────────────────────────────────────────── */
extern UART_HandleTypeDef huart1;   /* Debug / USB-UART bridge, 115200 baud   */
extern UART_HandleTypeDef huart6;   /* RS232 telemetry link,   9600 baud      */

/* ── Sensor limits ─────────────────────────────────────────────────────────── */
#define MAX_SENSOR_A        6   /* Max Sensor-A temperature sensors across all buses  */
#define MAX_SENSOR_B     4   /* Max Sensor-B accelerometers across all buses    */
#define MAX_I2C_BUSES   4   /* Number of physical I2C peripherals used        */

/* ── Sample rates (Hz) ─────────────────────────────────────────────────────── */
#define SENSOR_B_SAMPLE_HZ   100     /* Accelerometer — highest rate            */
#define SENSOR_A_SAMPLE_HZ       10     /* Temperature sensors                     */
#define SENSOR_C_SAMPLE_HZ      10     /* Humidity sensor                         */
#define GPS_SAMPLE_HZ         5     /* GPS — I²C bridge                        */
#define LOG_SD_HZ            10     /* SD card write rate                      */
#define LOG_RS232_HZ          2     /* RS232 telemetry packet rate             */
#define SENSOR_SCAN_HZ        1     /* Hot-plug I²C scan (every 2 s effective) */

/* Derived periods in milliseconds */
#define SENSOR_B_PERIOD_MS   (1000U / SENSOR_B_SAMPLE_HZ)
#define SENSOR_A_PERIOD_MS      (1000U / SENSOR_A_SAMPLE_HZ)
#define SENSOR_C_PERIOD_MS     (1000U / SENSOR_C_SAMPLE_HZ)
#define GPS_PERIOD_MS       (1000U / GPS_SAMPLE_HZ)
#define LOG_SD_PERIOD_MS    (1000U / LOG_SD_HZ)
#define LOG_RS232_PERIOD_MS (1000U / LOG_RS232_HZ)
#define SCAN_PERIOD_MS      2000U   /* Scan every 2 seconds                    */

/* ── Packet framing ────────────────────────────────────────────────────────── */
/* Magic byte values are defined at integration time and not published here.   */
#define PKT_HEADER_0    /* defined at integration */  0x00
#define PKT_HEADER_1    /* defined at integration */  0x00
#define PKT_HEADER_2    /* defined at integration */  0x00
#define PKT_FOOTER_0    /* defined at integration */  0x00
#define PKT_FOOTER_1    /* defined at integration */  0x00
#define PKT_FOOTER_2    /* defined at integration */  0x00
#define PKT_MAX_BYTES   70      /* Maximum serialised packet size (bytes)      */

/* ── SD card raw-sector layout ─────────────────────────────────────────────── */
/* Reserved sector addresses are configured at integration time.               */
#define SD_SECTOR_META_0        0U    /* Sector counter backup copy 0           */
#define SD_SECTOR_META_1        0U    /* Sector counter backup copy 1           */
#define SD_SECTOR_META_2        0U    /* Sector counter backup copy 2           */
#define SD_SECTOR_FLIGHT_NUM    0U    /* Flight / session counter               */
#define SD_SECTOR_DATA_START    0U    /* First writable data sector             */
#define SD_MAX_SECTORS    8388608UL   /* 4 GB card @ 512 B/sector               */

/* ── GPS data validity ──────────────────────────────────────────────────────── */
#define GPS_VALIDITY_TIMEOUT_S  5   /* Seconds before GPS fix is declared stale */

/* ── Sentinel values ────────────────────────────────────────────────────────── */
#define INVALID_FLOAT        (-999.0f)
#define INVALID_I2C_ADDR     (0xFFU)
#define INVALID_BUS_IDX      (0xFFU)

/* ── FreeRTOS task priorities (0 = idle, configMAX_PRIORITIES-1 = highest) ─── */
#define TASK_PRIO_ACCEL      6   /* Sensor-B — 100 Hz, tightest deadline        */
#define TASK_PRIO_GPS        5   /* GPS — time-critical for packet timestamps   */
#define TASK_PRIO_TEMP       4   /* Sensor-A + Sensor-C — 10 Hz                       */
#define TASK_PRIO_LOGGER     3   /* SD + RS232 transmit                         */
#define TASK_PRIO_WATCHDOG   2   /* IWDG feed — must never starve               */
#define TASK_PRIO_SCAN       1   /* Hot-plug scan — background                  */

/* ── FreeRTOS task stack sizes (words, 1 word = 4 bytes on Cortex-M7) ───────── */
#define STACK_ACCEL     256
#define STACK_GPS       512   /* Larger: NMEA string parsing                   */
#define STACK_TEMP      256
#define STACK_LOGGER    512   /* Larger: FatFS + packet serialisation           */
#define STACK_WATCHDOG  128
#define STACK_SCAN      256

/* ── Watchdog reload period ─────────────────────────────────────────────────── */
#define WATCHDOG_FEED_PERIOD_MS  500U   /* Must be < IWDG timeout (~800 ms)      */

#endif /* APP_CONFIG_H */
