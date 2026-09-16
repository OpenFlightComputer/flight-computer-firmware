# Flight blackbox

The flight blackbox records the same canonical control sample used by the USB
control trace, without adding a second diagnostic construction path to the
flight-control task. Recording starts as soon as initialized SD storage and a
canonical diagnostic sample are available, so startup calibration, the
pre-arm attitude, and the complete arm transition are retained. Receiver-loss
`FAILSAFE` remains in the same log. A fault closes the log immediately; a
normal disarm retains a one-second 100 Hz tail before closing it.

Blackbox capture is disabled while a USB CDC host is configured. If USB
enumerates after a pre-arm recording has already started, the current log is
closed through the normal queued writer and no new log starts until USB is
disconnected. Bench diagnostics can therefore use the control trace, while
completed standalone logs remain listable and downloadable. Flight Computer
V1 cannot reliably sense physical VBUS, so this policy deliberately uses the
actual CDC configured state rather than the defective PA9 VBUS divider.
Standalone pre-arm capture begins after a two-second USB-enumeration grace
period, or immediately if the vehicle reaches `ARMED`, `FAILSAFE`, or `FAULT`.
This prevents ordinary USB-powered boots from consuming one log-index slot
before the host has had time to enumerate.

## Real-time boundary

The 1 kHz flight task calls `flight_diagnostics_capture()` once. The call
returns immediately when neither diagnostic consumer is active. During
startup, flight, failsafe, and the post-disarm tail it copies a sample into a
fixed RAM sector buffer at 100 Hz. Steady disarmed operation is sampled at 10 Hz.
Completed
512-byte sectors enter a fixed 32-sector queue; a background, low-priority SD
task advances one SPI1 DMA transfer at a time. The flight task never performs
SPI, waits for the SD card, formats JSON, or allocates memory. If the card
cannot keep up, new samples are dropped and the count is stored in the flight
footer and log index. A constant-time due check runs before assembling the
canonical diagnostic sample, so the steady disarmed path does not copy the
large record at the 1 kHz control-task rate merely to retain it at 10 Hz.

The board backend uses the hardware mapping validated by the manufacturing
tester: SPI1 on PA5/PA6/PA7, PC4 chip select, and active-low PC5 card detect.
Initialization runs at 328.125 kHz and data transfers at 21 MHz. DMA2 Stream 0
(RX) and Stream 3 (TX), channel 3, are intentionally separate from the motor
DMA stream.

## Raw format

This first-flight implementation deliberately uses raw sectors rather than a
filesystem. Sectors 0 and 1 contain redundant, CRC-protected index copies.
Flight data begins at sector 2 and consists of independently CRC-protected,
versioned blocks:

1. flight header with format version, sample interval, firmware version/build,
   start time, and configuration length/CRC;
2. the complete stable binary flight-configuration snapshot;
3. version-2 control samples containing raw and filtered IMU data, estimator
   stages, receiver/setpoint values, desired rates, P/I/D terms, mixer results,
   motor commands, effective takeoff-leveling targets, motor baseline,
   takeoff-leveling/calibration states, timestamps, validity, and event flags;
4. footer with end time, captured/dropped counts, and final state.

The numeric takeoff-leveling states are stable within format 2: `0` disabled,
`1` capturing at zero throttle, `2` actively transitioning toward level, `3`
complete, and `4` frozen after the first nonzero throttle but still below the
configured transition threshold.

`storage initialize` writes only the two blackbox index sectors. It does not
format a filesystem or require firmware to be copied to the card, but it makes
previous raw blackbox logs unreachable and therefore requires explicit host
confirmation. Normal firmware flashing does not erase the SD card.

## USB and CLI

Storage management is disarmed-only:

```bash
./ofc storage status
./ofc storage initialize --yes
./ofc flight-log list
./ofc flight-log download latest --json-output latest-flight.json
./ofc flight-log download 3 --output flight-3.ofcb
./ofc flight-log decode flight-3.ofcb
```

Downloads read one sector per correlated JSON request. The raw `.ofcb` file
preserves the exact versioned on-card bytes; decoding validates every block
CRC before producing JSON. The host decoder accepts both the prior version-1
sample layout and the current version-2 layout. The SD card therefore remains
installed in the aircraft for normal retrieval.

Connecting USB closes any active pre-arm or standalone recording. Wait for
`storage status` to report `READY` before listing or downloading logs.

`storage status` reports the current and maximum queue depth, completed sector
writes, and average/maximum sector-write latency in addition to captured and
dropped sample counts. These values distinguish temporary card stalls from a
sustained throughput shortfall without adding work to the flight task.

Zero-drop 100 Hz recording is physically validated on a standalone 18.41-second
run; power-loss recovery remains a hardware-validation item before first
flight. A future batched multi-block writer may restore the 500 Hz target
without changing the on-card format or flight capture boundary.

The current format indexes at most 16 logs. Physical testing showed that
attempting a seventeenth recording enters `ERROR` before capturing data and
also prevents log reads until reboot. Development workflows must archive and
initialize storage before reaching that limit. Phase 5 must replace this with
a bounded retention policy that safely reclaims the oldest completed log,
reports every overwrite, and never sacrifices read access merely because the
index is full.
