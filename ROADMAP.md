# Roadmap

Development is milestone-driven. Each milestone is implemented, documented, verified, and reviewed before the next begins.

## Phase 0 — Firmware foundation

1. Repository initialization — complete.
2. STM32F405 build and boot foundation — complete; physical-board verification remains recorded.
3. Board and MCU hardware-layer skeleton — complete.
4. Monotonic microsecond timebase — complete.
5. Task abstraction — complete.
6. Cooperative scheduler — complete.
7. Application state machine — complete.
8. Fault system — complete.
9. Non-blocking logging core — complete.
10. USB CDC logging backend — implemented; awaiting owner review.
11. USB newline-delimited JSON command foundation.
12. Structured health reporting.
13. Integration review.

Phase 0 does not control motors, decode receiver input, or use sensor data for flight.

## Later phases

| Phase | Objective |
| --- | --- |
| 1 | DShot actuator subsystem controlled by safe USB bench commands |
| 2 | ELRS/CRSF receiver input, normalization, freshness, and diagnostics |
| 3 | Open-loop receiver-to-motor integration and first controlled physical response |
| 4 | BMI270-based estimation and stabilized flight, followed by optional BMP388 use |
| 5 | Non-blocking structured microSD flight-data logging |
| 6 | Optional external peripherals such as GPS |
| 7 | Evidence-driven Flight Computer V2 review |

Simulation, GUI configuration, autonomous navigation, computer vision, a bootloader, and any RTOS migration are later evidence-driven work rather than part of the current foundation.
