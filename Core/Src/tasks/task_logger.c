/**
 * @file    tasks/task_logger.c
 * @brief   SD card + RS232 telemetry logger task — 10 Hz / 2 Hz.
 *
 * On each 100 ms wake:
 *   1. Snapshot both global data structs under their mutexes (minimise hold time).
 *   2. Build a TelemetryPacket_t from the snapshot.
 *   3. Write the serialised packet to the SD card raw sector.
 *   4. Every 5th wake (= 2 Hz): transmit over RS232 via USART6.
 *
 * SD hot-plug is handled by checking the SD_DETECT pin each iteration.
 * If the card is removed and reinserted, sd_writer_init() is called again
 * to re-mount without rebooting — important for field deployments.
 */

#include "tasks/task_logger.h"
#include "shared_data.h"
#include "packet.h"
#include "app_config.h"
#include "sd_writer.h"
#include "main.h"

/* RS232 sub-sampling: transmit every (LOG_SD_HZ / LOG_RS232_HZ) iterations */
#define RS232_SUBSAMPLE  (LOG_SD_HZ / LOG_RS232_HZ)

extern uint16_t g_flight_number;   /* Initialised in main.c from SD */

void task_logger(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(LOG_SD_PERIOD_MS);

    uint8_t          rs232_counter = 0;
    TelemetryPacket_t pkt;
    uint8_t           raw[PKT_MAX_BYTES];
    SensorData_t      sensor_snap;
    GPS_Data_t        gps_snap;

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* ── Snapshot under mutexes (hold time: memcpy only) ────────────── */
        if (LOCK_SENSOR_DATA(10))
        {
            sensor_snap = globalSensorData;     /* struct copy */
            UNLOCK_SENSOR_DATA();
        }
        else continue;

        if (LOCK_GPS_DATA(10))
        {
            gps_snap = globalGPSData;
            UNLOCK_GPS_DATA();
        }
        /* GPS snapshot failure is non-fatal — continue with stale data */

        /* ── Build + serialise packet ────────────────────────────────────── */
        packet_build(&pkt, &sensor_snap, &gps_snap, g_flight_number);
        uint16_t len = packet_serialise(&pkt, raw);

        /* ── SD card write ───────────────────────────────────────────────── */
        if (sd_writer_check_card())
        {
            sd_writer_write_packet(raw, len);
        }

        /* ── RS232 transmit (sub-sampled to LOG_RS232_HZ) ────────────────── */
        if (++rs232_counter >= RS232_SUBSAMPLE)
        {
            rs232_counter = 0;
            HAL_UART_Transmit(&huart6, raw, len, 100);
        }
    }
}
