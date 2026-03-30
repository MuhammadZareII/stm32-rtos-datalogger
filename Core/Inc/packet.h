/**
 * @file    packet.h
 * @brief   Binary telemetry packet: layout, serialisation API, and CRC.
 *
 * The same packet is written to the SD card raw sectors and transmitted over
 * the RS232 link at LOG_RS232_HZ.  The receiver on the ground-station PC
 * deserialises the same struct.
 *
 * Wire layout (fixed 70 bytes, little-endian):
 * ┌──────────────────────────────────────────────────────────────────┐
 * │ Byte(s) │ Field                                                  │
 * ├─────────┼────────────────────────────────────────────────────────┤
 * │  0–2    │ Header  0xAA 0xBB 0xCC                                 │
 * │  3–6    │ Timestamp  uint32_t  HAL_GetTick() ms since boot       │
 * │  7–8    │ Flight number  uint16_t                                │
 * │  9      │ LM75 count  uint8_t                                    │
 * │ 10–33   │ LM75[0..5]  float temperature (4 B each)  × 6         │
 * │ 34      │ ADXL count  uint8_t                                    │
 * │ 35–58   │ ADXL[0..3]  float ax, ay, az (12 B each) × 4         │
 * │ 59      │ SHT30 present  uint8_t                                 │
 * │ 60–63   │ SHT30 humidity_pct  float                             │
 * │ 64      │ GPS fix_type  uint8_t                                  │
 * │ 65–68   │ GPS latitude  float                                    │
 * │ ... (truncated to PKT_MAX_BYTES with footer)                    │
 * │ 67–69   │ Footer  0xDD 0xEE 0xFF                                 │
 * └──────────────────────────────────────────────────────────────────┘
 *
 * Notes:
 * - All floats are IEEE 754 single precision.
 * - Unused sensor slots are filled with INVALID_FLOAT (0xC077800000).
 * - The packet is padded to PKT_MAX_BYTES so the receiver can use fixed
 *   offsets without parsing variable-length fields.
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
    uint8_t  header[3];             /**< { 0xAA, 0xBB, 0xCC }                  */

    /* Timing */
    uint32_t timestamp_ms;          /**< HAL_GetTick() at packet build time     */
    uint16_t flight_number;         /**< Increments on each power cycle         */

    /* Temperature */
    uint8_t  lm75_count;
    float    lm75_temp[MAX_LM75];   /**< °C; INVALID_FLOAT if slot unused       */

    /* Acceleration */
    uint8_t  adxl_count;
    float    adxl_ax[MAX_ADXL345];
    float    adxl_ay[MAX_ADXL345];
    float    adxl_az[MAX_ADXL345];

    /* Humidity */
    uint8_t  sht30_present;
    float    sht30_humidity;        /**< % RH                                   */
    float    sht30_temperature;     /**< °C (SHT30 internal sensor)             */

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
    uint8_t  footer[3];             /**< { 0xDD, 0xEE, 0xFF }                  */
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
