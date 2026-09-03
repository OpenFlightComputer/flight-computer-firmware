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
10. USB CDC logging backend — complete.
11. USB newline-delimited JSON command foundation — complete.
12. Structured health reporting — complete.
13. Integration review — complete.

Phase 0 does not control motors, decode receiver input, or use sensor data for flight.

## Phase 1 — DShot actuator subsystem

1. Motor command model — complete.
2. Generic actuator/motor interface — complete.
3. V1 bring-up carryover and flight-firmware smoke test — in progress: the
   board-selected VBUS mode and bounded 64-bit formatting are implemented;
   build traceability and physical foundation-image validation remain.
4. Hardware-independent DShot packet encoder.
5. TIM8/GPIO/DMA board mapping review against the physical V1.
6. DShot timing representation and rate selection.
7. Final lifecycle, health, freshness, and force-stop safety gate.
8. Single-channel timer/DMA output.
9. Four-channel synchronized output.
10. USB manual motor commands through the shared command model.
11. Propeller-free ESC and motor bench validation.

Phase 1 does not route receiver data to motors or implement stabilization.

## Later phases

| Phase | Objective |
| --- | --- |
| 1 | DShot actuator subsystem controlled by safe USB bench commands — in progress |
| 2 | ELRS/CRSF receiver input, normalization, freshness, and diagnostics |
| 3 | Open-loop receiver-to-motor integration and first controlled physical response |
| 4 | BMI270-based estimation and stabilized flight, followed by optional BMP388 use |
| 5 | Non-blocking structured microSD flight-data logging |
| 6 | Optional external peripherals such as GPS |
| 7 | Evidence-driven Flight Computer V2 review |

After first-flight capability exists, add a separate evidence-driven vehicle
condition and flight-phase layer. It may infer conditions such as `LANDED`,
`IN_FLIGHT`, and `CRASHED`, plus a higher-level readiness/health assessment
based on functional checks. Its exact state model, confidence requirements,
sensor inputs, and control authority must be designed from flight evidence; it
is not required for first flight and is not part of the current deterministic
lifecycle state machine.

Simulation, GUI configuration, autonomous navigation, computer vision, a bootloader, and any RTOS migration are later evidence-driven work rather than part of the current foundation.

The manufacturing acceptance run provides implementation evidence that future
milestones must carry over deliberately: BMI270 SPI3 and BMP388 I2C2 settings,
sensor timestamps/freshness and recovery policy, active-low microSD detect and
the proven two-speed SPI initialization sequence, and bounded/non-blocking
storage. The V1 discrete red/green LEDs are unusable and must not become safety
indicators. WS2812 output is restricted to non-flight operation until its
approximately 30 microsecond interrupt-masking implementation is accepted by
timing measurement or replaced by a proven timer/DMA backend. Details and
evidence are tracked in `docs/v1-bringup-carryover.md`.

Once independently versioned host software such as a GUI must communicate with
older deployed firmware, define protocol-version identifiers, compatibility
rules, and migration behavior. USB authentication or cryptography is likewise
deferred until after first-flight capability and a concrete threat model; the
current USB interface remains a physically trusted bench/development channel.
