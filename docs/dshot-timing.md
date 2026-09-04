# DShot300 timing and DMA buffer

Milestone 1.6 converts four already-encoded DShot frames into the interleaved
timer compare values required by the selected Flight Computer V1 TIM8 DMA
burst. It is a pure representation milestone: it does not configure GPIO,
TIM8, DMA, interrupts, or motor power.

## Initial ESC validation target

Initial propeller-free hardware testing targets the **SpeedyBee BLS 60A
30x30 4-in-1 ESC** supplied in the SpeedyBee F405 V4 BLS 60A stack. The
manufacturer specifies:

- stock firmware: BLHeli_S `J-H-40`;
- supported signal protocols: DShot300 and DShot600;
- supply: 3S through 6S LiPo;
- separate ESC telemetry output: not supported;
- current sensor: supported, with the manufacturer's Betaflight reference
  values scale `400` and offset `0`; and
- the included 1000 uF low-ESR capacitor is strongly recommended across the
  battery input.

Sources:

- <https://www.speedybee.com/speedybee-f405-v4-bls-60a-30x30-fc-esc-stack/>
- <https://store-fhxxhuiq8q.mybigcommerce.com/product_images/img_SpeedyBee_F405_V4_60A_Stack/manual/speedybee_f405v460a_stack_manual_en.pdf>

The ESC connector maps `S1` through `S4` to the board's `ESC_M1` through
`ESC_M4`. `CURRENT` maps to the board current-sense path, and the ESC's `N/A`
telemetry position must remain unused. The first implementation uses ordinary
unidirectional DShot with the request-telemetry bit clear.

The included SpeedyBee flight controller is not part of the OpenFlightComputer
signal path. Its motor outputs must not be connected in parallel with the V1
board outputs.

## Selected rate

DShot300 is selected for initial bring-up. At the recorded 168 MHz TIM8 input:

```text
timer ticks per bit = 168,000,000 / 300,000 = 560
bit period          = 560 / 168,000,000 = 3.333... us
0 high time         = 560 * 3/8 = 210 ticks = 1.25 us
1 high time         = 560 * 6/8 = 420 ticks = 2.50 us
```

TIM8 will therefore use a period of 560 timer ticks (`ARR = 559` when the
register setup is added). Compare value 210 represents a zero bit, and 420
represents a one bit.

DShot600 is intentionally a roadmap item, not a selectable current profile.
After DShot300 is physically validated, DShot600 can be added as another timing
profile. The frame encoder and interleaved-buffer algorithm will remain
unchanged; only the bit rate and derived timer values change.

## Buffer layout

`dshot_dma_buffer_t` contains 18 rows of four 16-bit compare values:

```text
rows 0..15  = frame bits 15..0, most-significant bit first
rows 16..17 = all-zero trailing low periods
```

Each row is one future TIM8 update burst. Its columns are timer compare
register order, not aircraft motor order:

| Buffer lane | Timer destination | V1 physical signal |
| --- | --- | --- |
| 0 | CCR1 | `ESC_M4` |
| 1 | CCR2 | `ESC_M3` |
| 2 | CCR3 | `ESC_M2` |
| 3 | CCR4 | `ESC_M1` |

The two zero rows force every PWM output low after the frame. Exact DMA
pipeline startup and completion behavior is deferred until TIM8/DMA activation;
the hardware milestone must not assume these rows alone prove a safe stop.

## Worked 50% and 25% examples

The examples use the reviewed normalized mapping:

```text
positive DShot value = 48 + round(throttle * (2047 - 48))
```

Therefore:

```text
50%: 48 + round(0.50 * 1999) = 1048
     encoded frame = 0x830B = 1000 0011 0000 1011

25%: 48 + round(0.25 * 1999) = 548
     encoded frame = 0x4488 = 0100 0100 1000 1000
```

“50%” is a normalized command, not a guarantee of 50% electrical power or
thrust.

All four physical motors at 50% produce four identical columns:

| Bit | CCR1/M4 | CCR2/M3 | CCR3/M2 | CCR4/M1 |
| ---: | ---: | ---: | ---: | ---: |
| 15 | 420 | 420 | 420 | 420 |
| 14 | 210 | 210 | 210 | 210 |
| 13 | 210 | 210 | 210 | 210 |
| 12 | 210 | 210 | 210 | 210 |
| 11 | 210 | 210 | 210 | 210 |
| 10 | 210 | 210 | 210 | 210 |
| 9 | 420 | 420 | 420 | 420 |
| 8 | 420 | 420 | 420 | 420 |
| 7 | 210 | 210 | 210 | 210 |
| 6 | 210 | 210 | 210 | 210 |
| 5 | 210 | 210 | 210 | 210 |
| 4 | 210 | 210 | 210 | 210 |
| 3 | 420 | 420 | 420 | 420 |
| 2 | 210 | 210 | 210 | 210 |
| 1 | 420 | 420 | 420 | 420 |
| 0 | 420 | 420 | 420 | 420 |
| low 1 | 0 | 0 | 0 | 0 |
| low 2 | 0 | 0 | 0 | 0 |

For physical M1/M2 at 50% and M3/M4 at 25%, the reverse channel routing means
the 25% frames occupy CCR1/CCR2 and the 50% frames occupy CCR3/CCR4:

| Bit | CCR1/M4 25% | CCR2/M3 25% | CCR3/M2 50% | CCR4/M1 50% |
| ---: | ---: | ---: | ---: | ---: |
| 15 | 210 | 210 | 420 | 420 |
| 14 | 420 | 420 | 210 | 210 |
| 13 | 210 | 210 | 210 | 210 |
| 12 | 210 | 210 | 210 | 210 |
| 11 | 210 | 210 | 210 | 210 |
| 10 | 420 | 420 | 210 | 210 |
| 9 | 210 | 210 | 420 | 420 |
| 8 | 210 | 210 | 420 | 420 |
| 7 | 420 | 420 | 210 | 210 |
| 6 | 210 | 210 | 210 | 210 |
| 5 | 210 | 210 | 210 | 210 |
| 4 | 210 | 210 | 210 | 210 |
| 3 | 420 | 420 | 420 | 420 |
| 2 | 210 | 210 | 210 | 210 |
| 1 | 210 | 210 | 420 | 420 |
| 0 | 210 | 210 | 420 | 420 |
| low 1 | 0 | 0 | 0 | 0 |
| low 2 | 0 | 0 | 0 | 0 |

The second table is retained as an exact host-test vector.

## API guarantees and limits

The timing profile accepts only DShot300 today and requires an exactly
representable timer period and duty fractions. Invalid rates, clocks, pointers,
or externally corrupted profiles are rejected. Profile creation does not
replace the caller's previous profile on failure, and invalid buffer requests
do not modify the previous buffer.

The module consumes complete encoded 16-bit frames. It deliberately does not
recompute or validate their checksum, authorize motor output, translate floats,
own a DMA buffer, or decide when frames are transmitted. It copies the four
source frames before writing, so source storage may overlap the destination
buffer without corrupting later lanes or bits.

Host tests prove the timing arithmetic, MSB-first conversion, four-lane
interleaving, the worked mixed-throttle table, every bit/lane combination,
trailing zeros, and rejection behavior. They do not prove TIM8 registers, DMA
bursts, pin waveforms, voltage levels, ESC recognition, motor order, or motor
direction.
