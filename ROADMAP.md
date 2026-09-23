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

## Phase 1 — DShot motor subsystem

1. Motor command model — complete.
2. Generic actuator/motor interface — complete.
3. V1 bring-up carryover and flight-firmware smoke test — complete for planned
   implementation and initial smoke evidence: the
   board-selected VBUS mode and bounded 64-bit formatting are implemented;
   the initial Debug-image smoke test passes; dirty-aware build identity and
   reusable `./ofc` automation are implemented; V1's WS2812 now receives a
   deterministic boot-off frame after an undriven input caused random green
   illumination; remaining stress/boundary checks stay tracked as pre-flight
   validation.
4. Hardware-independent DShot packet encoder — complete; all values and
   telemetry states are exhaustively host-tested.
5. TIM8/GPIO/DMA board mapping review against the physical V1 — complete;
   fixed routes, grouped DMA resources, and a safely configurable logical
   assignment are host-tested.
6. DShot timing representation and rate selection — complete; DShot300 selected
   for the initial SpeedyBee BLS 60A/J-H-40 ESC.
7. Final lifecycle, health, freshness, and force-stop safety gate — complete.
8. TIM8/DMA output engine — combined with Milestone 1.9 by owner decision;
   four-channel implementation and host/build verification complete.
9. Four-channel synchronized output — implemented with Milestone 1.8 because
   all channels share one table, timer, DMA stream, and completion path.
10. USB manual motor commands through the shared command model — complete.
11. Propeller-free ESC and motor bench validation — complete; DShot300,
    continuous stop-frame preparation, retained-command 1 kHz output,
    synchronized four-channel output, and the default physical motor mapping
    are verified with the initial SpeedyBee ESC.

Phase 1 does not route receiver data to motors or implement stabilization.

After DShot300 is physically reliable, add DShot600 as an optional timing
profile. It is supported by the selected SpeedyBee ESC, but it is not a
first-flight prerequisite and must not be selectable until separately tested.

## Phase 2 — ELRS/CRSF receiver input

Phase 2 is split so the flight application can progress while the manufacturing
tester independently proves the physical receiver path:

1. Tester-to-flight receiver boundary — complete; the portable source returns
   at most one complete decoded 16-channel frame per non-blocking call.
2. Tester-proven CRSF parser and V1 UART backend import — implementation
   complete, including the physically proven absolute DMA producer/consumer
   accounting and overrun-detection correction.
3. Receiver service and owned raw-channel snapshot — complete and registered
   as a 1 kHz high-priority task below motor output.
4. Channel assignment, replaceable calibration, normalized control snapshot,
   and configurable freshness — complete and wired to the physical source.
5. Connection/loss transitions, bounded diagnostics, and non-critical Phase 2
   fault reporting — complete; the staged policy is observed but has no motor
   or lifecycle authority until Phase 3.
6. USB inspection and flight-firmware physical validation against the receiver
   — complete; the flashed flight image and tester-style host view displayed
   live RP1 raw channels, normalized controls, freshness, and diagnostics.

Phase 2 never submits motor commands. Receiver-loss authority over `FAILSAFE`
begins in Phase 3 when the receiver becomes a motor-command source.
Phase 2 is complete.

The initial configurable receiver-loss policy will retain the last controls
through the short validation window, then use neutral roll/pitch/yaw and 5%
normalized throttle during Stage 1 before the configured Stage 2 deadline.
This is only a deterministic development fallback: 5% throttle is not a
measured hover or climb setting, and neutral rate commands cannot guarantee a
level climb. After stabilized attitude control exists, determine a safe
vehicle-specific hover/light-climb throttle from physical testing and replace
the Stage 1 fallback with a validated level/stabilized recovery behavior.

## Phase 3 — Open-loop receiver-to-motor integration

1. Exclusive motor-command source authority — complete; a
   successful arm latches `USB_TEST` or `RECEIVER`, and every stop path releases
   it without adding lifecycle states.
2. Safe receiver arming and disarming with startup-low and low-throttle
   interlocks — complete; receiver motor submission remains prohibited.
3. Runtime-safe motor direction configuration — complete in software and
   incorporated into the unified flight configuration; physical persistence
   across a normal flash remains to be checked.
4. Hardware-independent open-loop quad-X mixer — implemented for review.
5. Dedicated receiver-control producer task through the existing motor gate —
   implemented for review.
6. Receiver-loss policy authority and explicit Stage 2 recovery — implemented
   for review; propeller-free physical validation remains.
7. End-to-end control/authority USB inspection — deferred until after first
   flight. A combined debugger view should correlate receiver input, failsafe
   decisions, normalized controls, mixed motor values, command ownership, and
   motor-command age; the existing receiver, status, configuration, and log
   views are sufficient for the initial physical validation.
8. Propeller-free receiver-to-motor and loss/recovery validation.

Phase 3 does not provide attitude stabilization or flight readiness. Those
require IMU-based estimation and closed-loop control in Phase 4.

## Phase 4 — Stabilization and first hover

The minimum first-flight control mode is self-leveling angle control for roll
and pitch, gyroscope rate control for roll, pitch, and yaw, configurable maximum
angles and rotation rates, roll/pitch/yaw expo, a dedicated throttle curve and
maximum-throttle limit, and small stick deadbands. Controller correctness also
requires bounded PID corrections, integral anti-windup, basic gyro filtering,
and fail-closed IMU timestamp/freshness handling.

Implement this in small independently validated steps: port the tester-proven
BMI270 SPI3 transport, publish timestamped body-axis samples, add physical IMU
inspection and stationary gyro calibration, estimate roll/pitch attitude,
implement the angle and rate controllers, replace the open-loop receiver path,
then complete propeller-free correction/failure tests before a constrained
first-hover attempt.

Planned milestones:

1. BMI270 dependency, generic SPI boundary, V1 SPI3 transport, sensor
   initialization, and one raw boot sample — implemented in software; physical
   flight-image validation remains.
2. Scheduled raw acquisition with owned timestamps, freshness, and body-axis
   mapping — implemented in software; physical rate, timing, and sign
   validation remains.
3. IMU validation and calibration:
   - 4.3a request-driven USB inspection and tester-style host visualization —
     implemented in software; physical axis/rate/timing validation remains.
   - 4.3b stationary gyro calibration — implemented in software; physical
     startup-lock, calibration, and lifecycle-indicator validation remains.
   - Startup-only sample consumption now belongs to a temporary 1 kHz startup
     task, which disables itself after the successful `DISARMED` transition.
4. Roll/pitch attitude estimation with bounded basic gyro filtering —
   implemented in software; physical estimate sign, convergence, timing, and
   motion validation remains.
5. Configurable input curves, deadbands, angle/rate limits, and throttle curve
   — implemented in software; physical stick-response validation remains.
6. Bounded rate PID control with derivative-on-measurement, anti-windup,
   timestamp continuity handling, and an explicit disabled-state reset —
   implemented in software; gains remain unvalidated and motor integration is
   intentionally deferred to 4.8.
7. Self-leveling roll/pitch outer loop feeding the three-axis rate controller —
   implemented in software.
8. Replace open-loop receiver mixing with the stabilized control path and a
   fail-closed IMU gate — implemented in software; physical validation remains.
9. Propeller-free sign, correction, saturation, loss, and recovery validation,
   supported by a 100 Hz automatic microSD blackbox — implemented and
   physically validated on the secured vehicle, including corrected roll and
   pitch feedback, receiver loss, terminal-state capture, and zero-drop
   standalone recording. Power-loss recovery remains open. The original 500 Hz
   target requires batched SD multi-block writes and remains deferred.
   Automatic task-context UART/DMA recovery for receiver startup noise is
   implemented; cold-power-order recovery still requires physical validation.
10. Constrained first-hover preparation and test. The initial always-selected
    Easy mode adds configurable armed idle, launch-attitude capture and an
    explicit first-nonzero-throttle freeze followed by
    a bounded transition to level, and pre-arm blackbox coverage. Physical
    propeller-free validation remains before the next lift-off.

After the control behavior has been physically validated, perform a
behavior-preserving readability pass over the flight-control coordinator.
Extract its authority/failsafe handling, setpoint preparation, stabilization,
mixing, and motor-submission stages into small named functions or modules;
validate inputs once at clear module boundaries, retain explicit fail-closed
decisions, and keep diagnostic-specific branching outside the flight
algorithms. This cleanup is recorded work but is not a first-hover blocker.

Explicitly defer setpoint slew limiting until physical control tests show that
expo plus angle/rate limits are insufficient. Also defer acrobatic/rate-only
flight modes and their supporting airmode behavior. Easy mode now commands a
configurable equal armed-idle output at exact zero stick while resetting the
rate controller; disarm remains a complete motor stop. It does not add
free-fall-specific leveling limits on
throttle recovery, acceleration-magnitude gating of accelerometer correction,
or a dedicated gyro-only free-fall estimator mode. Revisit these together when
an aggressive flight mode is designed.

Altitude hold, BMP388 control authority, advanced or dynamic filters,
automatic PID tuning, multiple rate profiles, automatic takeoff and landing,
and position control also remain deferred until after first-flight capability.
The combined receiver/control/motor debugger view remains deferred as recorded
in Phase 3.

## Flight-control architecture evolution

1. Canonical control boundary — implemented in software. The central 1 kHz
   control core consumes `vehicle_state_t`, `control_objective_t`, and a
   `prepared_control_profile_t`, then produces controller and mixer output
   without depending on the receiver or motor authority layer. The current
   receiver path is retained as the first behavior adapter so this refactor
   does not intentionally change physical motor behavior.
2. Manual receiver behavior — implemented in software. Manual Easy has its own
   receiver/failsafe/vehicle-state input signature, owns takeoff-leveling state,
   and produces only the canonical objective. Arming, lifecycle transitions,
   core execution, motor authority, submission, and diagnostics remain outside
   the behavior. It remains the only selected behavior, so this extraction does
   not intentionally change motor output.
3. Behavior selection — implemented in software. Schema 11 selects exactly one
   named behavior through the complete JSON profile while disarmed. The flight
   task resets behavior/core state after an accepted profile replacement and
   explicitly dispatches the selection; every behavior still produces only a
   canonical objective. Manual Easy is the sole valid selection until another
   complete behavior is introduced.
4. Supervised autonomous hop — planned after the manual path proves the
   arbitration boundary. This is deliberately only partly autonomous: the
   receiver remains connected and the pilot retains unconditional arm/disarm
   authority. A dedicated start AUX switch begins one bounded hop while the
   arm switch remains the immediate cancellation control. The behavior uses
   the same vehicle state, objectives, profiles, central control core, motor
   gate, and diagnostics as Manual Easy.

   The behavior starts in armed idle and requires a fresh low-to-high edge on
   the dedicated start switch. Arming alone must never begin the sequence. It
   then observes a configurable start delay, increases throttle linearly from
   armed idle using elapsed monotonic time and a configurable rate, stops at a
   separately configurable maximum, optionally holds that maximum for a
   bounded interval, and ramps back to armed idle. It requests level roll and
   pitch and zero yaw rate throughout. Completion leaves the vehicle at armed
   idle until the pilot moves the arm switch to disarmed; another run requires
   a deliberate new start edge and must not retrigger from a switch that was
   already high.

   Moving the arm switch to disarmed at any point bypasses the ramp and causes
   the existing lifecycle and motor-safety path to command an immediate motor
   stop. Receiver loss, stale/invalid IMU data, excessive attitude, an invalid
   objective, or expiration of a hard sequence deadline must also abort rather
   than applying the ordinary receiver flight-fallback throttle. No behavior
   state may delay or veto those decisions.

   Initial settings are intentionally incapable of takeoff. Validation first
   proves the state machine, measured-time ramp, limits, logging, and every
   abort path with propellers removed. Propeller-equipped tests then begin
   with a maximum safely below expected liftoff and raise that maximum only
   through an explicit disarmed configuration change after the previous log
   has been reviewed. Later tests progress to making the vehicle barely light
   and only then to a short hop. The firmware never learns or raises the
   maximum automatically. Without altitude feedback this produces a
   reproducible command profile, not a guaranteed repeatable altitude.

## Near-term completion plan

1. Finish physical Phase 4 validation without adding unrelated capability.
   Confirm estimator and feedback signs, timing and freshness gates, receiver
   normalization, motor mapping, armed idle, disarm, receiver loss, and
   complete blackbox capture.
2. Implement the supervised autonomous-hop behavior and its fixed-capacity
   configuration, keeping lifecycle ownership and the central flight core
   outside the behavior.
3. Validate it progressively: host tests, propeller-free tests, below-liftoff
   tests, barely-light tests, and finally short hops. Preserve representative
   configuration/build/log sets as physical reference data.
4. Freeze the resulting observable behavior and perform a dedicated cleanup:
   remove obsolete experiments, correct stale documentation, simplify
   coordinators, strengthen module boundaries, and retain the reference tests
   and blackbox behavior as regression evidence.

Simulation work proceeds separately and does not expand these firmware
milestones. The hop data is intended to provide a deterministic input schedule
and physical output evidence for later simulator comparison.

## Later phases

| Phase | Objective |
| --- | --- |
| 1 | DShot motor subsystem controlled by leased USB bench commands — complete |
| 2 | ELRS/CRSF receiver input, normalization, freshness, and diagnostics |
| 3 | Open-loop receiver-to-motor integration and first controlled physical response |
| 4 | BMI270-based estimation and stabilized flight, followed by optional BMP388 use |
| 5 | Post-flight blackbox evolution: retention policy, richer events, compression, and optional filesystem/export improvements |
| 6 | Optional external peripherals such as GPS |
| 7 | Evidence-driven Flight Computer V2 review |

The blackbox catalog no longer has the former fixed 16-log terminal failure:
format 2 pages 20,480 descriptors from the card while retaining one catalog
page in RAM, exposes catalog-full explicitly, and never silently overwrites a
completed log. Phase 5 may add an opt-in bounded retention policy that safely
reclaims the oldest completed log, exposes every overwrite through diagnostics,
preserves recoverable in-progress data across power loss, and remains
compatible with host-side download and archival.

After first-flight capability exists, add a separate evidence-driven vehicle
condition and flight-phase layer. It may infer conditions such as `LANDED`,
`IN_FLIGHT`, and `CRASHED`, plus a higher-level readiness/health assessment
based on functional checks. Its exact state model, confidence requirements,
sensor inputs, and control authority must be designed from flight evidence; it
is not required for first flight and is not part of the current deterministic
lifecycle state machine.

Simulation, GUI configuration, autonomous navigation, computer vision, and any
RTOS migration are later evidence-driven work rather than part of the current
foundation. A disarmed-only resident USB update path is complete: it
stops motor output, uses a reset-safe one-shot handoff, protects persistent
configuration from the host, verifies the image, and confirms the running
build ID. Retain SWD as the recovery path. Authentication, signed images,
rollback, and update-image compatibility rules remain later work.

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

Before first flight, physically validate loss of the 100 ms motor-command
heartbeat and record actual stop latency. Also add a disarmed-only runtime ESC
direction configuration so each motor can be reversed without recompiling or
changing wiring; direction commands must require zero throttle, use the
ESC-required repetition sequence, and be verified before arming.
