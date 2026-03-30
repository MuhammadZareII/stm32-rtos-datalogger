# stm32-rtos-datalogger

FreeRTOS-based multi-sensor data logger for STM32H7.

Reads temperature (Sensor-A), acceleration (Sensor-B), humidity (Sensor-C), and GPS (I²C DDC) from a custom hardware board with I²C bus extenders, logs binary telemetry packets to a raw SD card, and transmits over RS232 to a ground-station computer.

> **Note:** This repository is a structural reference implementation showing the RTOS task architecture. The peripheral initialisation (CubeMX-generated `MX_*` functions) and the proprietary SD raw-sector writer are intentionally not included. The sensor drivers are complete register-level implementations.

---

## Hardware

| Component          | Details                                                   |
|--------------------|-----------------------------------------------------------|
| MCU                | STM32H7 series (Cortex-M7)                               |
| I²C buses          | 4 × I²C buses, extended via I²C multiplexers             |
| Temperature        | Up to 6 × Sensor-A sensors across all buses                  |
| Accelerometer      | Up to 4 × Sensor-B sensors across all buses               |
| Humidity           | 1 × Sensor-C humidity sensor                                |
| GPS                | GPS module via I²C DDC bridge                            |
| Storage            | SDMMC — raw sector writes (no filesystem)                 |
| Telemetry          | USART → RS232 converter → ground station PC              |
| Debug              | USART / USB-CDC                                           |
| Watchdog           | IWDG                                                      |

---

## RTOS Task Architecture

```
┌──────────────────────────────────────────────────────────┐
│  Priority 6 │  task_accel   │  Sensor-B   │  100 Hz       │
│  Priority 5 │  task_gps     │  GPS        │    5 Hz       │
│  Priority 4 │  task_temp    │  Sensor-A+Sensor-C │   10 Hz       │
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

Fixed-size binary packet, little-endian, with a 3-byte header and 3-byte footer for framing.

Fields (in order): timestamp, flight/session counter, temperature readings (×6 slots), accelerometer readings (×4 slots, three axes each), humidity and humidity-sensor temperature, GPS fix, position, altitude, speed, UTC time, and a failure bitmask.

See `Core/Inc/packet.h` for the full struct definition. The same packet is written to raw SD sectors and transmitted over RS232.

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
    ├── drivers/              ← Sensor-A, Sensor-B, Sensor-C, GPS I²C implementations
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
