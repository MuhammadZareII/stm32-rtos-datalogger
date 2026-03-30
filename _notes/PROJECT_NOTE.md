# Project Note — stm32-rtos-datalogger
> Internal reference for portfolio writing. Not published to GitHub.

---

## The Story

This was a paid contract project for a client who needed a compact data acquisition system to be mounted on a platform (the exact application is confidential). The brief was: collect temperature, vibration, humidity, and GPS position data simultaneously from a custom hardware board, log it to an SD card, and relay it in real-time to a ground station over RS232.

The hardware was designed by the client. My role was firmware only. The board used a STM32H743IITx as the main processor — a 480 MHz Cortex-M7 with a hardware FPU, chosen for its peripheral density and the 1 MB SRAM needed to buffer sensor data safely. The I²C sensor bus was extended using PCA9548A multiplexers, which is why the firmware supports up to 6 temperature sensors and 4 accelerometers despite the I²C address space limitations.

The first delivered firmware was a bare-metal super-loop (which became `H743IITX_data_logger_v5` in the development history). It worked reliably in testing. The client signed off on it. However, I always knew the timing architecture was fragile — the 100 Hz ADXL345 read was being done on a best-effort basis rather than with hard guarantees, and the watchdog was fed in the main loop which meant a slow SD write could delay it.

The RTOS refactor is the version I would build if I were doing it again from scratch, or if the client were to request a firmware revision. The core code (sensor drivers, packet format, SD layout) is unchanged; only the scheduling architecture is different.

The GitHub repo (`stm32-rtos-datalogger`) is the RTOS version, deliberately written at a structural level — enough for another developer to understand the architecture, use the drivers, and port the task design to their own project — without being the exact proprietary firmware delivered to the client.

---

## What the System Does

1. **Sensor scan (0.5 Hz)**: Probes all four I²C buses for LM75, ADXL345, and SHT30 devices. Newly found devices are added to the active sensor list. Devices that disappear (power loss, broken connector) are removed. This hot-plug capability was a hard requirement from the client.

2. **Accelerometer read (100 Hz)**: 4 × ADXL345 sampled at 100 Hz each. The client needed vibration spectra up to 50 Hz (Nyquist), hence the 100 Hz rate. The ADXL345 was configured in ±4g full-resolution mode (7.8 mg/LSB).

3. **GPS read (5 Hz)**: u-blox GPS module polled via I²C DDC interface. NMEA sentences ($GPRMC for position/speed/time, $GPGGA for altitude/satellites) parsed in the driver. GPS UTC time is embedded in every packet to provide absolute time reference.

4. **Temperature + humidity (10 Hz)**: LM75 temperature sensors (0.5 °C resolution) and one SHT30 humidity sensor with its own temperature channel.

5. **SD card logging (10 Hz)**: Binary packets written to raw SD sectors at 512 bytes per sector. No filesystem — `f_write()` would add FatFS overhead and wear-levelling unpredictability. Sector pointers are saved to three redundant metadata sectors after each write session.

6. **RS232 telemetry (2 Hz)**: Same binary packet transmitted at 2 Hz over USART6 → RS232 converter to the ground station. The ground station runs a Python receiver that parses the packets and displays live graphs.

7. **Watchdog (2 Hz)**: IWDG fed every 500 ms. Timeout ~800 ms. Resets the MCU on any deadlock or runaway condition.

---

## Key Technical Details

### I²C Extender Topology

The PCA9548A is an 8-channel I²C switch. One chip per bus, controlled by a GPIO chip select. Each channel exposes a subset of sensors, preventing address collisions. The firmware's scan task probes each bus independently — the multiplexer management happens at the hardware layer and is transparent to the driver code.

This is why `SensorID_t` carries both `i2c_addr` and `bus_idx`. The same address (e.g. LM75 at 0x48) can exist on all four buses simultaneously as four physically distinct sensors.

### vTaskDelayUntil vs HAL_GetTick Polling

The bare-metal code tracked each sensor's `last_read` timestamp and fired reads only when `HAL_GetTick() - last_read >= period_ms`. This is polling — it has jitter equal to one loop iteration. If the loop takes 5 ms, the 10 Hz sensor gets sampled at 10 ms ± 5 ms.

`vTaskDelayUntil` works differently. It records the *absolute intended wake time* and computes the next period from that, not from the actual wake time. If the task runs 1 ms late, the next period is shortened by 1 ms to compensate. The result is that the long-term average rate is always exactly right, and short-term jitter is bounded to one RTOS tick (1 ms at 1000 Hz SysTick).

For the 100 Hz ADXL345 task this matters: over 10 seconds, the bare-metal approach might accumulate 50 ms of drift; `vTaskDelayUntil` accumulates 0.

### Mutex Timeout Strategy

Every mutex acquisition uses a short timeout (`LOCK_SENSOR_DATA(5)` = 5 ms) rather than `portMAX_DELAY`. If the timeout fires, the task simply skips that iteration. This is a deliberate trade-off:
- We might lose one sample at 100 Hz (undetectable in the data)
- We never risk priority inversion causing permanent starvation
- The watchdog continues to be fed

The scan task holds the mutex for up to 8 ms during a full bus sweep. At 100 Hz this means one missed ADXL sample per scan every 2 seconds — acceptable.

### SD Raw Sector Layout

The client required that data be recoverable even if the device is powered off mid-write. The solution:
- Sector counter is saved to three separate metadata sectors (101, 103, 104) and a flight-number sector (102)
- On startup, all three copies are read; the newest valid copy wins (triple redundancy)
- On shutdown or reset, all three are updated atomically (write 0, write 1, write 2 in sequence — any partial failure leaves at least two valid copies)
- The IWDG reset path does not corrupt data because the SD write in the logger task completes before the watchdog fires (SD write ~5 ms, IWDG timeout ~800 ms)

### Binary Packet Format

The packet was designed in collaboration with the client's ground station developer. Requirements:
- Fixed size (no length parsing)
- Visually identifiable in hex dump (magic bytes)
- Directly castable to a C struct on the receiver side
- All floats IEEE 754 single precision (both sides are x86/ARM little-endian)

The `#pragma pack(1)` ensures no padding. On the receiver (Python):
```python
import struct
pkt = struct.unpack('<3B I H B 6f B 12f B ff ff ff B B 10s B 3B', raw)
```

### GPS I²C DDC Interface

The u-blox DDC interface is essentially UART-over-I²C. The module maintains an internal FIFO; the host reads a 16-bit "bytes available" register, then reads that many bytes from a data register. Bytes arrive as a continuous NMEA stream.

The firmware accumulates bytes into a line buffer across calls, so partial sentences are handled correctly. The parser only handles `$GPRMC` and `$GPGGA` — all other NMEA sentence types are silently discarded.

Early versions used UART for GPS (the `pulling_mode_gps.h` in v2/v3). Switched to I²C in v4 to free a UART for RS232.

---

## Skills Demonstrated

| Domain | Skill |
|---|---|
| Embedded Systems | STM32H7 peripheral configuration (I²C, UART, SPI, SDMMC, TIM, IWDG) |
| RTOS | FreeRTOS task design with `vTaskDelayUntil` for precise multi-rate scheduling |
| RTOS | Mutex-based shared state protection, bounded timeout strategy |
| Firmware Architecture | Separation of driver layer, task layer, and data model |
| Sensor Drivers | LM75 (I²C register), ADXL345 (I²C burst read, full-res mode), SHT30 (command-response + CRC) |
| GPS | NMEA parsing ($GPRMC, $GPGGA), u-blox I²C DDC interface |
| Storage | Raw SD sector I/O, triple-redundant sector pointer persistence |
| Communication | Binary packet protocol, RS232 telemetry, USB-CDC debug |
| Hardware Interface | I²C bus extender topology (PCA9548A), hot-plug sensor discovery |
| Reliability | IWDG watchdog design, SD hot-plug recovery, missed-sample-tolerant scheduling |

---

## Original Source Files (for reference)

These are in `C:\Users\zareii\Desktop\Projects\projects\` — do not publish.

| Original | What it became |
|---|---|
| `H743IITX_data_logger_v5/Core/Inc/sensors.h` | `sensor_types.h` |
| `H743IITX_data_logger_v5/Core/Inc/app_config.h` (implicit) | `app_config.h` |
| `H743IITX_data_logger_v5/Core/Inc/log.h` | `packet.h` + `task_logger.h` |
| `H743IITX_data_logger_v5/Core/Inc/sensors_read_drivers.h` | `drivers/lm75.h`, `drivers/adxl345.h`, `drivers/sht30.h` |
| `H743IITX_data_logger_v5/Core/Inc/i2c_mode_gps.h` | `drivers/gps_i2c.h` |
| `H743IITX_data_logger_v5/Core/Inc/sensors_scan.h` | `tasks/task_scan.h` |
| `H743IITX_data_logger_v5/Core/Src/main.c` (super-loop) | `main.c` (RTOS task creation) |
| `F746GZ_GPS/Core/Src/main.c` | Early GPS prototype (UART mode) — not used |

The evolution from v1 to v5:
- **v1**: Basic SDMMC + single LM75
- **v2**: Added GPS (UART pulling mode), multiple LM75 + ADXL345
- **v3**: SHT30, refined scan logic, loop timing improvements
- **v4**: GPS moved to I²C DDC (freed UART for RS232), RS232 added
- **v5**: IWDG watchdog, MPU config, loop duration measurement, USB debug

---

## Portfolio Descriptions

### Short version

> Firmware engineer for a custom STM32H743-based multi-sensor data acquisition system. Designed and delivered a bare-metal C firmware reading LM75 temperature, ADXL345 accelerometer, SHT30 humidity, and GPS data over I²C bus extenders, logging binary telemetry packets to raw SD sectors and relaying to a ground station over RS232. Later refactored to FreeRTOS with per-sensor task scheduling and mutex-protected shared state.

### Paragraph version

> This was a contracted firmware project for a custom sensor-logging platform built around the STM32H743IITx. The hardware combined four I²C buses with PCA9548A multiplexers, allowing up to ten independent sensors to coexist on a single board. The firmware — written from scratch in C using STM32 HAL — handled hot-plug sensor discovery at 0.5 Hz, multi-rate sampling (100 Hz accelerometers, 10 Hz temperature, 5 Hz GPS), binary packet serialisation, raw SD card logging with triple-redundant sector pointers, and RS232 telemetry to a ground-station PC. The project also involved co-designing the ground-station receiver protocol. The public RTOS version on GitHub demonstrates the same architecture ported to FreeRTOS, using `vTaskDelayUntil` for jitter-free multi-rate scheduling and mutexes for safe inter-task data sharing.

### Academic framing

> This project addresses the real-time scheduling challenge of simultaneous multi-rate sensor acquisition on a resource-constrained embedded platform. The initial firmware used a timestamp-polling super-loop, which provided functional but timing-uncertain sampling. The refactored FreeRTOS architecture replaces this with `vTaskDelayUntil`-based periodic tasks, achieving bounded jitter (≤1 ms) at all sample rates. Shared sensor data is protected by priority-inheriting mutexes with short timeouts to prevent starvation, and a dedicated watchdog task ensures system recovery from any deadlock condition. The project demonstrates practical application of RTOS concepts — task priority assignment, inter-task communication, stack sizing — on a real deployed system rather than a toy example.

---

## Reflections

- **DMA for I²C**: All I²C transactions currently use blocking `HAL_I2C_Master_Transmit/Receive`. On a 480 MHz processor this is fine, but using DMA would reduce CPU load and allow the I²C hardware to run without waking the processor.
- **SHT30 non-blocking**: The 20 ms measurement wait in the SHT30 driver blocks the entire temp task for 20 ms. A state machine (trigger on one iteration, read on the next) would eliminate this.
- **SD FatFS option**: Raw sector writes avoid FatFS overhead but produce unreadable cards. A hybrid approach (FatFS for the first session, fallback to raw if unmount fails) would make field data extraction easier.
- **RTOS trace**: Never added a FreeRTOS trace hook (e.g. Segger SystemView). Adding one would allow visualising actual task execution and jitter in the profiler — valuable for proving the timing guarantees hold under load.
- **GPS RTCM / UBX**: The NMEA parser only handles two sentence types. Adding UBX binary protocol support would give access to raw satellite measurements for post-processing differential GPS if the application ever requires cm-level accuracy.
