# stm32-rtos-datalogger

FreeRTOS-based multi-sensor data logger for the STM32H743IITx.

Reads temperature (LM75), acceleration (ADXL345), humidity (SHT30), and GPS (u-blox I²C DDC) from a custom hardware board with I²C bus extenders, logs binary telemetry packets to a raw SD card, and transmits over RS232 to a ground-station computer.

> **Note:** This repository is a structural reference implementation showing the RTOS task architecture. The peripheral initialisation (CubeMX-generated `MX_*` functions) and the proprietary SD raw-sector writer are intentionally not included. The sensor drivers are complete register-level implementations.

---

## Hardware

| Component          | Details                                                   |
|--------------------|-----------------------------------------------------------|
| MCU                | STM32H743IITx (Cortex-M7, 480 MHz)                       |
| I²C buses          | 4 × (I2C1–I2C4), extended via PCA9548A multiplexers      |
| Temperature        | Up to 6 × LM75A on any bus (addresses 0x48–0x4F)         |
| Accelerometer      | Up to 4 × ADXL345 on any bus (addresses 0x53 / 0x1D)     |
| Humidity           | 1 × SHT30 on I2C3 (address 0x44)                         |
| GPS                | u-blox NEO-M8N via I²C DDC bridge on I2C3                 |
| Storage            | SDMMC1 — raw sector writes (no filesystem)                |
| Telemetry          | USART6 → RS232 converter → ground station PC              |
| Debug              | USART1 / USB-CDC                                          |
| Watchdog           | IWDG1 (~800 ms timeout)                                   |

---

## RTOS Task Architecture

```
┌──────────────────────────────────────────────────────────┐
│  Priority 6 │  task_accel   │  ADXL345   │  100 Hz       │
│  Priority 5 │  task_gps     │  GPS        │    5 Hz       │
│  Priority 4 │  task_temp    │  LM75+SHT30 │   10 Hz       │
│  Priority 3 │  task_logger  │  SD + RS232 │ 10/2 Hz       │
│  Priority 2 │  task_watchdog│  IWDG feed  │    2 Hz       │
│  Priority 1 │  task_scan    │  I²C scan   │  0.5 Hz       │
└──────────────────────────────────────────────────────────┘
         │                            │
  sensor_data_mutex            gps_data_mutex
  (guards SensorData_t)    (guards GPS_Data_t)
```

All inter-task shared state is protected by FreeRTOS mutexes.  Sensor tasks write; the logger task reads snapshot copies.  Mutex hold times are bounded to a single struct copy.

---

## Packet Format

Binary, fixed 70 bytes, little-endian:

```
[0xAA 0xBB 0xCC] [timestamp:u32] [flight:u16]
[lm75_count:u8]  [lm75_temp×6:f32]
[adxl_count:u8]  [ax×4:f32] [ay×4:f32] [az×4:f32]
[sht30_present:u8] [humidity:f32] [temperature:f32]
[gps_fix:u8] [lat:f32] [lon:f32] [alt:f32] [speed:f32] [utc:10c]
[failure_code:u8]
[0xDD 0xEE 0xFF]
```

The same packet is written to raw SD sectors and transmitted over RS232.

---

## Repository Layout

```
Core/
├── Inc/
│   ├── app_config.h          ← all tunable constants
│   ├── sensor_types.h        ← data structures
│   ├── shared_data.h         ← global instances + mutex handles
│   ├── packet.h              ← packet layout + API
│   ├── drivers/              ← driver header files
│   └── tasks/                ← task header files
└── Src/
    ├── main.c                ← hardware init + task creation + scheduler start
    ├── shared_data.c         ← global instance definitions
    ├── packet.c              ← packet build + serialise
    ├── drivers/              ← LM75, ADXL345, SHT30, GPS I²C implementations
    └── tasks/                ← FreeRTOS task bodies
docs/
└── architecture.md
```

---

## Building

This project targets STM32CubeIDE. Open the `.ioc` file, regenerate code, then add the `Core/Src/` files to the build.

FreeRTOS is included via STM32CubeMX CMSIS-RTOS2 wrapper (`configUSE_PREEMPTION = 1`, `configUSE_MUTEXES = 1`).

---

## License

MIT — see [LICENSE](LICENSE).
