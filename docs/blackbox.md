# Flight blackbox

The flight blackbox records the same canonical control sample used by the USB
control trace, without adding a second diagnostic construction path to the
flight-control task. Recording starts automatically when the system becomes
`ARMED` and finishes after preserving the terminal control sample when
`DISARMED`, `FAILSAFE`, or `FAULT` is observed.

## Real-time boundary

The 1 kHz flight task calls `flight_diagnostics_capture()` once. The call
returns immediately when neither diagnostic consumer is active. During a
flight it copies a sample into a fixed RAM sector buffer at 100 Hz. Completed
512-byte sectors enter a fixed 32-sector queue; a background, low-priority SD
task advances one SPI1 DMA transfer at a time. The flight task never performs
SPI, waits for the SD card, formats JSON, or allocates memory. If the card
cannot keep up, new samples are dropped and the count is stored in the flight
footer and log index.

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
3. 100 Hz control samples containing raw and filtered IMU data, estimator
   stages, receiver/setpoint values, desired rates, P/I/D terms, mixer results,
   motor commands, states, timestamps, validity, and event flags;
4. footer with end time, captured/dropped counts, and final state.

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
CRC before producing JSON. The SD card therefore remains installed in the
aircraft for normal retrieval.

`storage status` reports the current and maximum queue depth, completed sector
writes, and average/maximum sector-write latency in addition to captured and
dropped sample counts. These values distinguish temporary card stalls from a
sustained throughput shortfall without adding work to the flight task.

Zero-drop 100 Hz recording and power-loss recovery remain hardware-validation
items before first flight. A future batched multi-block writer may restore the
500 Hz target without changing the on-card format or flight capture boundary.
