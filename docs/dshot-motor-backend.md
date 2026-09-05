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
        v physical order ESC_M1..ESC_M4
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

## Buffer ownership

The application adapter builds each normal 18-by-4 halfword table in physical
`ESC_M1`-through-`ESC_M4` order and owns a prebuilt stop table in that same
order. The board accepts a normal submission only while idle, copies every row
into its private static DMA buffer, and changes columns according to the V1
physical routes. DMA therefore never retains an application pointer. `BUSY`
returns before either the board DMA buffer or hardware is changed.

The source and board tables are row-major. For every row, the board converts
`[M1, M2, M3, M4]` into `[CCR1/M4, CCR2/M3, CCR3/M2, CCR4/M1]`. DMA sees the
resulting 72 consecutive board-owned halfwords. Each TIM8 update request
consumes four values through `TIM8->DMAR`; `DCR` selects CCR1 as the base and a
four-register burst, so one row updates CCR1 through CCR4 together. DMA uses
normal rather than circular mode and cannot autonomously repeat stale throttle.

## Timer and DMA configuration

- TIM8 input: 168 MHz, verified from APB2 at runtime.
- Prescaler: zero.
- Auto reload: 559, producing 560 ticks or 3.333 microseconds per bit.
- PWM mode 1 with compare preload on channels 1 through 4.
- Active-high push-pull AF3 on PC6 through PC9 at very-high GPIO speed.
- Timer DMA base: CCR1; burst length: four halfwords.
- DMA2 Stream 1, Channel 7, memory-to-peripheral, halfword widths, incrementing
  memory, fixed `TIM8->DMAR` peripheral address, very-high DMA priority.
- DMA transfer-complete, transfer-error, direct-mode-error, and FIFO-error
  detection at NVIC priority 1. TIM5 overflow remains priority 0 and USB
  remains priority 6.

At rest, PC6 through PC9 are ordinary GPIO outputs driven low. A submission
switches all four pins to AF3 only after zero compare values and the DMA source
are ready. The first timer period is deliberately low while the first table
row enters the CCR preloads. The two trailing zero rows ensure the active CCRs
are low when transfer completion stops TIM8 and returns the pins to GPIO-low.

## Completion, errors, and stop

Normal transfers are asynchronous. While DMA reads the board-owned table,
backend status is busy. The DMA interrupt disables the timer request, timer, and DMA,
loads zero compares, drives all pins low, and publishes either idle or error.
It performs no state-machine, fault-system, logging, or table-generation work.

The 1 kHz highest-priority `motor-control` task calls
`motor_control_synchronize()`. It observes asynchronous backend error status
in main context and converts it to the existing critical motor-output fault.
The task also bounds lifecycle, health, and 100 ms command-timeout response to
one additional millisecond.

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

They do not prove pin voltage, timer/DMA request behavior, preload timing,
waveform widths, synchronous edges, ESC recognition, force-stop latency, motor
order, or direction. Those require the propeller-free hardware procedure. The
first observation should be PC9/ESC_M1, followed by PC8, PC7, and PC6; the
implementation itself always treats the command and DMA burst as one atomic
four-channel operation.
