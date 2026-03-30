/**
 * @file    packet.c
 * @brief   Telemetry packet build, serialise, and validate implementation.
 */

#include "packet.h"
#include "app_config.h"
#include "stm32h7xx_hal.h"
#include <string.h>

void packet_build(TelemetryPacket_t       *pkt,
                  const SensorData_t      *sensors,
                  const GPS_Data_t        *gps,
                  uint16_t                 flight_num)
{
    memset(pkt, 0, sizeof(*pkt));

    /* Header / footer */
    pkt->header[0] = PKT_HEADER_0;
    pkt->header[1] = PKT_HEADER_1;
    pkt->header[2] = PKT_HEADER_2;
    pkt->footer[0] = PKT_FOOTER_0;
    pkt->footer[1] = PKT_FOOTER_1;
    pkt->footer[2] = PKT_FOOTER_2;

    /* Timing */
    pkt->timestamp_ms = HAL_GetTick();
    pkt->flight_number = flight_num;

    /* Temperature */
    pkt->lm75_count = sensors->lm75_count;
    for (uint8_t i = 0; i < MAX_LM75; i++)
        pkt->lm75_temp[i] = (i < sensors->lm75_count)
                            ? sensors->lm75[i].temperature_c
                            : INVALID_FLOAT;

    /* Acceleration */
    pkt->adxl_count = sensors->adxl_count;
    for (uint8_t i = 0; i < MAX_ADXL345; i++)
    {
        if (i < sensors->adxl_count)
        {
            pkt->adxl_ax[i] = sensors->adxl[i].ax;
            pkt->adxl_ay[i] = sensors->adxl[i].ay;
            pkt->adxl_az[i] = sensors->adxl[i].az;
        }
        else
        {
            pkt->adxl_ax[i] = pkt->adxl_ay[i] = pkt->adxl_az[i] = INVALID_FLOAT;
        }
    }

    /* Humidity */
    pkt->sht30_present     = sensors->sht30_present;
    pkt->sht30_humidity    = sensors->sht30_present
                             ? sensors->sht30.humidity_pct
                             : INVALID_FLOAT;
    pkt->sht30_temperature = sensors->sht30_present
                             ? sensors->sht30.temperature_c
                             : INVALID_FLOAT;

    /* GPS */
    if (gps)
    {
        pkt->gps_fix   = gps->fix_type;
        pkt->gps_lat   = gps->latitude;
        pkt->gps_lon   = gps->longitude;
        pkt->gps_alt   = gps->altitude_m;
        pkt->gps_speed = gps->speed_kmh;
        strncpy(pkt->gps_utc, gps->utc, sizeof(pkt->gps_utc) - 1);
    }

    /* Health */
    pkt->failure_code = sensors->failure_code;
}

uint16_t packet_serialise(const TelemetryPacket_t *pkt, uint8_t *buf)
{
    /* The struct is already packed (#pragma pack(push,1)) — memcpy is safe */
    uint16_t len = (uint16_t)sizeof(TelemetryPacket_t);
    if (len > PKT_MAX_BYTES) len = PKT_MAX_BYTES;
    memcpy(buf, pkt, len);
    return len;
}

uint8_t packet_validate(const uint8_t *buf, uint16_t len)
{
    if (len < 6) return 0;
    /* Check header */
    if (buf[0] != PKT_HEADER_0 || buf[1] != PKT_HEADER_1 || buf[2] != PKT_HEADER_2)
        return 0;
    /* Check footer */
    if (buf[len-3] != PKT_FOOTER_0 || buf[len-2] != PKT_FOOTER_1 || buf[len-1] != PKT_FOOTER_2)
        return 0;
    return 1;
}
