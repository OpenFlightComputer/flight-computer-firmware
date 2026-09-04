# Flight Computer V1 bring-up carryover

This document converts the completed manufacturing acceptance run into flight-
firmware work. Passing in the tester is hardware evidence, not proof that the
current flight image configures or exercises the same path.

## Evidence baseline

- Board: Flight Computer V1, manufacturing revision 1.7, schematic revision 0.1.
- Tester acceptance implementation: commit `1642cde5f7bb`.
- Archived clean acceptance state: commit
  `c1fdcdfc540c7ab174d45c4be9cba5e8c32cc68a`.
- Configuration: `configs/test/test-config-v006.json`.
- Result: `docs/example-results/flightcomputer-v1-successful-run.json`.

The tester repository was inspected locally. The hardware repository was not
available in the current workspace, so this update does not claim a new KiCad
or DRC review beyond the previously recorded design snapshot.

## Immediate carryover before DShot

| Finding | Current flight-firmware state | Required action |
| --- | --- | --- |
| The equal 100 kOhm VBUS divider produces about 1 V and prevents enumeration when hardware VBUS sensing is enabled. | Implemented: the generic board contract defines explicit assume-present and sense-input modes; V1 selects assume-present, does not configure PA9, and disables peripheral sensing. | Prove flight-image enumeration on V1 hardware. A corrected V2 selects sense-input in its board definition without changing USB transport/application code. |
| Embedded newlib-nano did not reliably format `%llu`; it produced malformed tester JSON. | Implemented: one bounded allocation-free converter now owns all project 64-bit decimal formatting, including padded canonical logs. Host tests cover zero, decimal boundaries, padding, and `UINT64_MAX`; the embedded images link without project `%llu` usage. | Inspect exact status, health, and log bytes from the target build. |
| Formal acceptance needs an exact, dirty-aware build identity. | No flight build revision is embedded. | Generate a bounded `git describe --always --dirty --abbrev=12` identity at build time and make it debugger-visible. Adding it to the public JSON schema requires a separate explicit protocol decision. |
| The tester must retain output until the transport accepts it. | The command processor already retains one complete response in a fixed 768-byte buffer and stops consuming commands while it is pending. | Audit this invariant on target under disconnect/reconnect and queue saturation; do not replace it with tester session policy. |
| The 16 MHz HSE, 168 MHz core, and 48 MHz USB clock were physically proven. | The flight image uses the same clock tree and treats failure as critical. | Keep the fail-closed startup policy explicitly: loss of the timing basis prevents flight. Verify the flight image reaches its running state on the accepted board. |

These items form Phase 1 Milestone 1.3. The software USB/formatting carryover is
implemented; build identity and physical smoke evidence remain. They precede
packet encoding because they affect the only current physical interface and
all timestamped diagnostics.

## Phase-specific carryover

### Phase 1 — actuators

- PC6, PC7, PC8, and PC9 are selected as TIM8 CH1 through CH4 AF3. Milestone
  1.5 recorded the reverse `ESC_M4` through `ESC_M1` ordering and selected one
  TIM8-update DMA2 Stream 1/Channel 7 burst into CCR1 through CCR4. The tester
  did not physically validate DShot output; rate, waveform, and end-to-end
  routing still require their owning implementation and bench milestones.
- The generic output facade and command model remain valid; no manufacturing-
  tester motor policy should be copied into them.
- Verify each physical channel with propellers removed, including exact stop,
  stale-command stop, force-stop override, reset behavior, and simultaneous
  four-channel updates.

### Phase 2 — receiver

- PC10/PC11 still permit UART4 or USART3. The acceptance run did not resolve
  this selection, so it remains a receiver-protocol/DMA design decision.

### Phase 4 — sensors

- BMI270: SPI3 on PB3/PB4/PB5 AF6, PD2 chip select, about 656.25 kHz from the
  42 MHz APB1 clock with prescaler 64, mode 0, MSB first. Accepted configuration
  was accelerometer 100 Hz, +/-2 g, normal averaging 4 and gyroscope 100 Hz,
  +/-2000 degrees/s. PB6/PC12 interrupt use remains to be designed.
- BMP388: I2C2 on PB10/PB11 AF4 open-drain, address `0x76`, 100 kHz. Accepted
  configuration was pressure enabled at 4x oversampling, temperature enabled
  without oversampling, 25 Hz ODR, normal mode, with about two seconds allowed
  for settling.
- Preserve the vendor driver's context contract. In particular,
  `bmp3_dev.intf_ptr` must point to valid retained storage rather than remain
  null.
- Define the vehicle body axes once and remap at the driver boundary. Timestamp
  every sample and expose freshness, saturation, communication failure,
  missed-deadline, retry, and reinitialization behavior. The tester's blocking
  initialization and 5 Hz display cadence are not flight-runtime designs.

### Phase 5 — storage

- Use SPI1 on PA5/PA6/PA7 AF5, PC4 chip select, and confirmed active-low PC5
  card detect. The accepted sequence used about 328.125 kHz for initialization
  and 21 MHz afterward.
- Reuse the proven protocol sequence, but never reuse destructive raw-sector
  test policy. Operational logging needs a filesystem or other recoverable,
  power-loss-tolerant format and must not block the control loop.

### Status indication and V2 hardware

- PB13/PB14 discrete red/green LEDs are physically confirmed inoperable on V1;
  software polarity changes cannot repair them. Do not assign them boot, arm,
  health, or fault authority.
- The PA1 WS2812 worked on the first board with GRB, MSB-first data. The accepted
  tester fallback drives GPIO with the DWT cycle counter and masks interrupts
  for roughly 30 microseconds per update. Flight firmware must either restrict
  updates to non-flight states and measure the latency impact, or implement and
  verify a timer/DMA path; the tester's attempted TIM2/PWM/DMA path failed for
  an undetermined reason.
- V2 should correct the USB VBUS-divider topology, discrete LED footprints, and
  preferably buffer the 3.3 V WS2812 data signal. A clean hardware DRC remains
  required in the hardware repository.

## Required flight-image smoke test

Milestone 1.3 is complete only when the flight image—not merely the tester—has
been flashed and the following evidence recorded:

1. SWD program/reset reaches the running boot status with the expected build ID.
2. HSE/PLL reports 168 MHz and USB CDC enumerates with V1 VBUS sensing disabled.
3. Status and health responses plus timestamped logs remain valid JSON across
   zero and large 64-bit values.
4. Fragmented commands, request IDs, response retention, disconnect/reconnect,
   and transmit saturation behave as documented.
5. TIM5 measures 1 MHz; its 64-bit extension is observed or accelerated across
   a 32-bit wrap without losing monotonicity.
6. Successful startup is `DISARMED`; injected critical startup failures enter
   terminal `FAULT`; non-critical USB failure does not prevent safe startup.

Sensor, storage, RGB, receiver, and motor checks remain in their owning
milestones even though the manufacturing tester has already established useful
hardware and implementation evidence for them.
