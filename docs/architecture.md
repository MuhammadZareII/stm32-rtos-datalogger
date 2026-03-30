# Architecture

## The Core Problem: Accurate Multi-Rate Timing

The central engineering challenge of this project is sampling sensors at different rates simultaneously without any sample jitter, while also writing to the SD card and transmitting RS232 telemetry at their own rates.

In the bare-metal version this was solved with a `last_read` timestamp in each sensor struct and a super-loop that polled `HAL_GetTick()` on every iteration. That approach works but has two weaknesses: the loop period depends on the slowest operation in it, and there is no guarantee that a 100 Hz sensor actually gets read at 100 Hz if a slow I²C transaction on another device runs long.

The RTOS version replaces the super-loop with one task per rate class, each using `vTaskDelayUntil()`.

---

## Task Design

### `task_accel` — 100 Hz, Priority 6

The highest-rate task. Uses `vTaskDelayUntil(&xLastWakeTime, 10ms)` which guarantees that the next wake-up is computed from the *intended* wake time, not the *actual* wake time. If the task overruns its slot by 1 ms, it still wakes 9 ms later (not 10 ms later), keeping the 100 Hz cadence precise.

I²C read time for 4 Sensor-B units at 400 kHz: ~800 µs. Mutex hold time: ~800 µs. Remaining slack: ~9.2 ms.

### `task_gps` — 5 Hz, Priority 5

GPS ranked above temperature because GPS timestamps are used to anchor all other data to real-world UTC time in the packet. A fresh GPS timestamp on every 200 ms cycle is more valuable than knowing the temperature to within the next 100 ms.

The GPS driver reads up to 512 bytes per call from the module's DDC FIFO, accumulating NMEA lines across calls. A single call typically processes 1–3 complete sentences.

### `task_temp` — 10 Hz, Priority 4

Handles both Sensor-A (simple register read) and Sensor-C (command-response with a 20 ms measurement delay). The Sensor-C measurement delay is handled by `HAL_Delay(20)` inside the driver — a known inefficiency that could be improved by splitting trigger and read into separate task iterations using a state machine.

### `task_logger` — 10 Hz SD / 2 Hz RS232, Priority 3

Critical design decision: the logger **never holds a mutex during SD write or UART transmit**. It takes a snapshot copy of both structs, releases the mutexes, then does all I/O with the local copies. This keeps sensor task preemption latency short.

RS232 sub-sampling: the task wakes at 10 Hz (LOG_SD_PERIOD_MS = 100 ms) but only transmits RS232 on every 5th wake (= 2 Hz), using a simple counter.

SD hot-plug: `sd_writer_check_card()` reads the SD_DETECT GPIO pin. If the card was absent and is now present, the writer calls `sd_writer_init()` to remount and continues from the last saved sector pointer.

### `task_watchdog` — 2 Hz, Priority 2

Feeds the IWDG every 500 ms. The watchdog is configured with enough margin that if the watchdog task is starved, the system resets — indicating a deadlock or runaway task.

### `task_scan` — 0.5 Hz, Priority 1

The lowest-priority real task. A full scan of all 4 buses takes 4 × 10 addresses × ~200 µs = ~8 ms. At 0.5 Hz this is negligible CPU time, but because it holds `sensor_data_mutex` during the entire scan, sensor tasks must wait up to 8 ms if they happen to wake during a scan. This is acceptable for the 10 Hz tasks (800 µs >> 8 ms... wait, this is 8 ms vs 100 ms budget), but for the 100 Hz task a 8 ms wait represents 80% of the 10 ms budget.

Mitigation: the scan task releases the mutex between each bus scan iteration, and sensor tasks use a 5 ms mutex timeout and simply skip the iteration if they can't acquire it. One missed sample at 100 Hz is acceptable.

---

## Shared Data Model

```
globalSensorData   ←── guarded by sensor_data_mutex
globalGPSData      ←── guarded by gps_data_mutex
```

Separate mutexes for sensors and GPS avoid the priority inversion scenario where `task_gps` (priority 5) is blocked by `task_logger` (priority 3) holding `sensor_data_mutex`.

Mutex acquisition uses a bounded timeout everywhere (never `portMAX_DELAY`) to ensure a stuck mutex never causes permanent task starvation.

---

## Packet Layout Rationale

The packet is `#pragma pack(1)` to eliminate padding and match the wire format exactly. `memcpy` from the struct directly into the transmit buffer is safe and correct.

Fixed-size packets (70 bytes) were chosen over variable-length because:
- The receiver can use fixed field offsets without parsing
- Raw SD sector writes are 512 bytes; exactly 7 packets fit per sector (7 × 70 = 490 bytes, 22 bytes padding)
- RS232 at 9600 baud can transmit one 70-byte packet in ~72 ms, well within the 500 ms period

A 3-byte header and 3-byte footer magic sequence bracket each packet, chosen to be easily identifiable in a hex dump and statistically unlikely to appear in valid float data.

---

## I²C Bus Architecture

The custom hardware uses 4 physical I²C buses, each expanded via I²C multiplexers to reach more sensors than the address limit of a single bus allows. The firmware treats each bus as independent; the scan task probes all buses in sequence.

At 400 kHz (Fast Mode), each bus can complete one 6-byte read in ~0.2 ms, giving a theoretical maximum of ~5000 reads/second across 4 buses. At the configured rates (100 Hz × 4 Sensor-B + 10 Hz × 6 Sensor-A = 460 reads/sec), bus utilisation is well below 10%.

---

## Bare-Metal → RTOS Migration Summary

| Aspect | Bare-metal (v5) | RTOS (this repo) |
|---|---|---|
| Timing mechanism | `last_read` timestamp polling in super-loop | `vTaskDelayUntil()` per-task |
| Timing accuracy | Loop-jitter dependent | ±1 RTOS tick (1 ms) |
| Shared state protection | None (single-threaded) | Mutex per data struct |
| SD write | Blocking in main loop | Dedicated logger task, non-blocking to sensors |
| Watchdog | Fed in main loop | Dedicated task, immune to logger stalls |
| Hot-plug scan | Runs every 2 s in main loop | Background task, lowest priority |
| Missed sensor reads on I²C error | Blocks loop briefly | Task skips one period, continues next |
