# Four-channel DShot300 backend

Milestones 1.8 and 1.9 are implemented together because Flight Computer V1
uses one TIM8 update-triggered DMA burst for all four motor outputs. The
backend is compiled and initialized at boot, but this milestone adds no USB,
receiver, or flight-control motor-command producer. Flashing this image cannot
request nonzero throttle.

## Complete data path

```text
motor_command_t in logical order
        |
        v
motor_control: lifecycle + health + validity + freshness + mapping
        |
        v retained physical order ESC_M1..ESC_M4
1 kHz motor-control service
        |
        v one fresh physical submission per millisecond
dshot_motor_backend: float -> value -> frame -> physical-order table
        |
        v 18 rows x ESC_M1..ESC_M4
board motor output: copy/reorder into its owned DMA table
        |
        v CCR1..CCR4 via DMA2 Stream 1 Channel 7 -> TIM8 DMAR
CCR1/PC6/M4, CCR2/PC7/M3, CCR3/PC8/M2, CCR4/PC9/M1
```

The application adapter is the only production caller of the DShot encoder
and timing-table APIs. It derives DShot300 from the board-reported 168 MHz
timer clock, but knows only physical `ESC_M1` through `ESC_M4` order. Pin,
channel, and CCR ordering remain entirely inside the selected board layer.

Positive normalized throttle uses the reviewed mapping:

```text
dshot value = 48 + round(throttle * 1999)
```

Exact normalized zero uses DShot value zero. Telemetry requests remain clear.

Direction configuration is a separate backend operation. `NORMAL` maps to
DShot command 20 and `REVERSED` to command 21, with the request bit set. One
submission contains the command for all four physical outputs in the same
synchronized 18-by-4 table used by throttle frames. The motor controller owns
the required ten repetitions and never mixes direction commands with a
nonzero throttle submission.

## Buffer ownership

The application adapter builds each normal 18-by-4 `uint16_t` table in physical
`ESC_M1`-through-`ESC_M4` order and owns a prebuilt stop table in that same
order. The board accepts a normal submission only while idle, copies every row
into its private static DMA buffer, and changes columns according to the V1
physical routes while widening every compare value into a `uint32_t` DMA
entry. DMA therefore never retains an application pointer. `BUSY`
returns before either the board DMA buffer or hardware is changed.

The source and board tables are row-major. For every row, the board converts
`[M1, M2, M3, M4]` into `[CCR1/M4, CCR2/M3, CCR3/M2, CCR4/M1]`. DMA sees the
resulting 72 consecutive board-owned words. Each TIM8 update request
consumes four values through `TIM8->DMAR`; `DCR` selects CCR1 as the base and a
four-register burst, so one row updates CCR1 through CCR4 together. DMA uses
normal rather than circular mode and cannot autonomously repeat stale throttle.

## Timer and DMA configuration

- TIM8 input: 168 MHz, verified from APB2 at runtime.
- Prescaler: zero.
- Auto reload: 559, producing 560 ticks or 3.333 microseconds per bit.
- Compare values: 210 ticks for zero and 420 ticks for one. The current
  isolation image restores the original 37.5%/75% duty profile.
- PWM mode 1 with compare preload on channels 1 through 4.
- Active-high push-pull AF3 on PC6 through PC9 at very-high GPIO speed.
- Timer DMA base: CCR1; burst length: four register transfers.
- DMA2 Stream 1, Channel 7, memory-to-peripheral, word widths, incrementing
  memory, fixed `TIM8->DMAR` peripheral address, very-high DMA priority.
- DMA transfer-complete, transfer-error, direct-mode-error, and FIFO-error
  detection at NVIC priority 1. TIM5 overflow remains priority 0 and USB
  remains priority 6.

After initialization or an explicit emergency stop, PC6 through PC9 are
ordinary GPIO outputs driven low and TIM8 is stopped. The 1 kHz motor task then
begins submitting stop frames in `DISARMED`; the first submission switches all four
pins to AF3 only after zero compare values and the DMA source are ready. Its
first timer period is deliberately low while the first table row enters the
CCR preloads. The two trailing zero rows ensure the active CCRs are low at
transfer completion. Successful normal completions leave TIM8 running and the
pins in AF3 with those zero compares; subsequent frames therefore require only
a new DMA table submission. DMA uses normal mode, so the timer cannot replay a
completed table while it waits for the next 1 kHz submission.

## Completion, errors, and stop

Normal transfers are asynchronous. While DMA reads the board-owned table,
backend status is busy. On successful completion, the DMA interrupt disables
the timer's update-DMA request and the DMA stream, then publishes idle while
leaving TIM8 and AF3 active at zero compare. On any interrupt validation or DMA
error it performs the full safe shutdown: TIM8 is stopped, compare values are
cleared, and all four pins become GPIO outputs driven low. The interrupt
performs no state-machine, fault-system, logging, or table-generation work.

The 1 kHz highest-priority `motor-control` task calls
`motor_control_synchronize()`. A command producer only replaces the retained
validated snapshot and renews its 100 ms lease. Every service release observes
the preceding asynchronous completion, rechecks lifecycle, health, and lease,
then starts one new frame when safe. This produces a nominal 1 kHz frame rate
independent of USB/radio update timing. A frame still busy after 1,000
microseconds or any asynchronous error becomes a critical motor-output fault
in main context.

The board retains only a compact failure reason for setup, DMA, interrupt, and
timeout failures. The application copies that reason into the existing motor
fault context before forcing the outputs low. Detailed peripheral-register and
frame-table inspection remains available through the debugger without adding a
permanent diagnostic protocol or duplicate runtime state.

Force-stop aborts any normal transfer, transmits the prebuilt four-motor DShot
zero frame synchronously with a bounded poll, and then leaves TIM8 stopped and
all four pins GPIO-low. It returns an error if DMA cannot be disabled, reports
an error, or fails to complete within the bound. This synchronous path delays
interrupts for approximately one DShot frame during a successful stop; its
actual duration and interaction with USB/timebase interrupts require physical
measurement.

## Verification boundary

Host tests prove normalized conversion, physical-order adapter output, the
board-specific physical-to-CCR transformation, complete and stop tables, busy
behavior, initialization failures, status/result mapping, and asynchronous-
error propagation through the safety gate. Debug and Release builds prove the
selected STM32 register names, interrupt symbol, and static linkage.

Physical testing confirmed DShot300 acceptance, synchronized four-channel
operation, and the default motor order on the initial SpeedyBee BLS 60A ESC.
Pin voltage, exact waveform widths, force-stop latency, heartbeat-loss behavior,
direction-command acceptance, persistent configuration, and final motor
directions still require separate measurement or propeller-free validation.
The implementation always treats the command and DMA burst as one atomic
four-channel operation.
