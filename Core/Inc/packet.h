/**
 * @file    packet.h
 * @brief   Binary telemetry packet: layout, serialisation API, and CRC.
 *
 * The same packet is written to the SD card raw sectors and transmitted over
 * the RS232 link at LOG_RS232_HZ.  The receiver on the ground-station PC
 * deserialises the same struct.
 *
 * The packet is fixed-size, little-endian, #pragma pack(1).  It begins with
 * a 3-byte header magic sequence and ends with a 3-byte footer magic sequence
 * (values defined in app_config.h).  All floats are IEEE 754 single precision.
 * Unused sensor slots are filled with INVALID_FLOAT.
 * The packet is padded to PKT_MAX_BYTES so the receiver can use fixed
 * offsets without parsing variable-length fields.
 */

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include "sensor_types.h"
#include "app_config.h"

/* ── Packet struct (mirrors wire layout) ───────────────────────────────────── */

#pragma pack(push, 1)   /* Ensure no padding bytes are inserted */

typedef struct {
    /* Header */
    uint8_t  header[3];             /**< PKT_HEADER_0/1/2 magic bytes           */

    /* Timing */
    uint32_t timestamp_ms;          /**< HAL_GetTick() at packet build time     */
    uint16_t flight_number;         /**< Increments on each power cycle         */

    /* Temperature */
    uint8_t  sensor_a_count;
    float    sensor_a_temp[MAX_SENSOR_A];   /**< °C; INVALID_FLOAT if slot unused       */

    /* Acceleration */
    uint8_t  sensor_b_count;
    float    sensor_b_ax[MAX_SENSOR_B];
    float    sensor_b_ay[MAX_SENSOR_B];
    float    sensor_b_az[MAX_SENSOR_B];

    /* Humidity */
    uint8_t  sensor_c_present;
    float    sensor_c_humidity;        /**< % RH                                   */
    float    sensor_c_temperature;     /**< °C (Sensor-C internal sensor)             */

    /* GPS */
    uint8_t  gps_fix;
    float    gps_lat;
    float    gps_lon;
    float    gps_alt;
    float    gps_speed;
    char     gps_utc[10];           /**< "HH:MM:SS\0"                           */

    /* Health */
    uint8_t  failure_code;          /**< Bitmask from SensorData_t              */

    /* Footer */
    uint8_t  footer[3];             /**< PKT_FOOTER_0/1/2 magic bytes           */
} TelemetryPacket_t;

#pragma pack(pop)

/* ── Public API ─────────────────────────────────────────────────────────────── */

/**
 * @brief  Build a TelemetryPacket_t from the current global sensor snapshot.
 *
 * Caller must hold sensor_data_mutex and gps_data_mutex before calling,
 * or pass explicit copies of the data.
 *
 * @param[out] pkt          Destination packet buffer.
 * @param[in]  sensors      Snapshot of sensor data.
 * @param[in]  gps          Snapshot of GPS data.
 * @param[in]  flight_num   Current flight/session counter.
 */
void packet_build(TelemetryPacket_t       *pkt,
                  const SensorData_t      *sensors,
                  const GPS_Data_t        *gps,
                  uint16_t                 flight_num);

/**
 * @brief  Serialise a packet to a raw byte buffer for SD/RS232 transmission.
 *
 * @param[in]  pkt   Source packet.
 * @param[out] buf   Destination buffer (must be >= PKT_MAX_BYTES bytes).
 * @return           Number of bytes written.
 */
uint16_t packet_serialise(const TelemetryPacket_t *pkt, uint8_t *buf);

/**
 * @brief  Validate header and footer magic bytes of a raw buffer.
 * @return  1 if valid, 0 if corrupted.
 */
uint8_t packet_validate(const uint8_t *buf, uint16_t len);

#endif /* PACKET_H */
