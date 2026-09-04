# Motor output mapping

Milestone 1.5 resolved the Flight Computer V1 motor-output resources and added
a separate configurable logical-to-physical assignment. Milestone 1.6 has now
selected DShot300 and added a pure timing-buffer representation, but still does
not configure GPIO, start TIM8 or DMA, send ESC commands, or energize a motor.

## Fixed physical V1 routing

Physical output indices zero through three correspond to schematic signals
`ESC_M1` through `ESC_M4`:

| Physical output | Schematic signal | MCU pin | Alternate function | Timer output |
| --- | --- | --- | --- | --- |
| 0 | `ESC_M1` | PC9 | AF3 | TIM8_CH4 |
| 1 | `ESC_M2` | PC8 | AF3 | TIM8_CH3 |
| 2 | `ESC_M3` | PC7 | AF3 | TIM8_CH2 |
| 3 | `ESC_M4` | PC6 | AF3 | TIM8_CH1 |

`hardware/boards/flightcomputer_v1/motor_output_map.*` is the firmware source
of truth for this fixed ordering. The reverse timer-channel order is explicit
and host-tested rather than being repeated in future register code.

The routing agrees with the retained V1 hardware map and manufacturing-test
configuration. The STM32F405 alternate-function table independently confirms
that PC6, PC7, PC8, and PC9 select TIM8_CH1 through TIM8_CH4 with AF3:

- STM32F405/407 datasheet DS8626:
  <https://www.st.com/resource/en/datasheet/dm00037051.pdf>
- STM32F405/407 reference manual RM0090:
  <https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf>

The board clock tree divides APB2 by two, producing an 84 MHz peripheral
clock. STM32 timer clock doubling therefore supplies TIM8 with 168 MHz.
Milestone 1.6 derives 560 timer ticks per DShot300 bit from this clock. See
`docs/dshot-timing.md` for the compare values and interleaved buffer order.

## Selected grouped DMA path

All four outputs share TIM8 and will use one update-triggered timer DMA burst:

| Resource | Selection |
| --- | --- |
| Timer | TIM8 |
| Timer clock | 168 MHz |
| DMA request | TIM8 update |
| DMA controller | DMA2 |
| DMA stream | Stream 1 |
| DMA channel selection | Channel 7 |
| Timer burst destination | CCR1 through CCR4 |
| Values per update | 4 |

RM0090's DMA2 request map assigns TIM8 update to DMA2 Stream 1, Channel 7.
TIM8's `DCR`/`DMAR` mechanism can redirect one timer-triggered DMA burst into
sequential timer registers. CCR preloading will be required so the four values
written during a burst become active together at a timer update boundary.
Those register writes and their initial/final pipeline behavior belong to the
timer-output milestones.

This choice uses one stream and one completion/error path instead of four
independent channel streams. It also avoids consuming DMA2 Streams 2, 3, 4,
and 7, retaining flexibility for future SPI1 and ADC transfers. ST's timer
cookbook describes timer DMA burst as the facility for changing multiple
channel registers from one timer event:

<https://www.st.com/resource/en/application_note/dm00236305-pwm-generation-using-stm32-general-purpose-timers-stmicroelectronics.pdf>

## Configurable logical assignment

The PCB routes are immutable, but aircraft motor positions are not embedded in
them. `flight/actuators/motor_mapping.*` owns a permutation from four logical
motors to the four physical outputs. The default is identity:

```text
logical 0 -> physical output 0 / ESC_M1
logical 1 -> physical output 1 / ESC_M2
logical 2 -> physical output 2 / ESC_M3
logical 3 -> physical output 3 / ESC_M4
```

A proposed runtime mapping is accepted only when both conditions supplied by
the future actuator owner are true:

1. the lifecycle state is `DISARMED`;
2. the physical output backend has accepted force-stop.

The mapping must be a complete permutation of `0, 1, 2, 3`. Duplicate,
missing, and out-of-range outputs are rejected without modifying the previous
mapping. A complete command is reordered through temporary local storage, so
in-place application is safe and no partially remapped motor set is exposed.

The module does not read application lifecycle state or operate hardware. The
future application-owned actuator adapter is responsible for deriving the two
safety conditions from the actual state machine and output backend rather than
passing optimistic values.

## Motor direction

Logical expected CW/CCW direction and the actual direction stored by each ESC
are separate concepts. Neither is guessed here. Expected direction belongs to
the later aircraft/mixer configuration. Changing an ESC's stored direction
requires an explicit disarmed maintenance operation, exact stopped output,
the ESC-specific DShot command sequence, and propeller-free confirmation.

## Resource review

- TIM5 remains an internal APB1 timebase and uses no GPIO or DMA stream.
- USB OTG FS uses PA11/PA12 and no DMA in the current device implementation.
- V1 WS2812 boot-off uses DWT-timed PA1 GPIO and no timer or DMA.
- Planned SPI3, I2C2, USART2, and UART4/USART3 resources do not use TIM8,
  PC6-PC9, or DMA2 Stream 1.
- Planned SPI1 and ADC activity can use other DMA2 streams; their final
  allocations must continue to respect this TIM8 reservation.
- DCMI, ADC3, and TIM1 functions that could share DMA2 Stream 1 are not routed
  as current V1 flight requirements.

## Verification boundary

Host tests prove the recorded route table, selected grouped resources,
permutation validation, disarmed/stopped configuration gate, atomic rejection,
and complete command reordering. They do not prove the PCB trace, alternate-
function register configuration, waveform timing, voltage, DMA execution, ESC
acceptance, motor order, or motor direction.

Physical validation remains staged and propeller-free: first observe a single
output waveform if practical, then identify one ESC/motor at a time, and only
then test synchronized four-channel output.
