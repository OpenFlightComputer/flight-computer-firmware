# Flight Computer V1 board support

This board implementation targets manufacturing revision 1.7, generated from schematic revision 0.1, using an STM32F405RGT6 and a 16 MHz HSE crystal.

`board.c` owns startup policy: initialize the selected MCU, require successful clock configuration, verify the expected 168 MHz system clock, and start the selected timebase. `board_definition.h` records stable board identity and clock facts plus the V1 choice of an 84 MHz TIM5 input, a 1 MHz counter, interrupt priority zero, a 168 MHz TIM8 input, and assume-present USB VBUS behavior.

`motor_output_map.*` records the fixed `ESC_M1` through `ESC_M4` routes on
PC9 through PC6 as reversed TIM8 channels 4 through 1, and the selected grouped
TIM8-update DMA2 Stream 1/Channel 7 burst into CCR1 through CCR4.
`board_motor_output.c` consumes those facts to configure the complete
four-channel DShot300 transfer, interrupt completion/error handling, and the
GPIO-low rest state. Its API accepts every compare row in physical
`ESC_M1`-through-`ESC_M4` order; the board reorders each row into CCR1-through-
CCR4 order and copies it into the private buffer subsequently read by DMA.

`board_receiver.c` owns the physically proven RadioMaster RP1 route: PC10 is
UART4 TX and PC11 is UART4 RX on alternate function 8. UART4 uses normal,
non-inverted 420000-baud 8-N-1 serial data from the 42 MHz APB1 clock. DMA1
Stream 2/Channel 4 continuously fills a 512-byte circular RX buffer. The 1 ms
high-priority application task parses independently of the background USB and
logging service. A transfer-complete interrupt increments the DMA wrap epoch;
the reader combines that epoch with `NDTR` to compare absolute producer and
consumer counts. The DMA-owned buffer and interrupt-owned epoch are volatile,
barriers make their ordering explicit, and backlog beyond 512 bytes is counted
as an overrun with the exact dropped-byte count before reading resumes at the
oldest retained byte. The buffer represents about 12.2 ms of wire time at
420000 baud.

`time.c` exposes the generic `time_us()` API without leaking the STM32 backend. Milestone 0.10 adds `usb_device_port.c`, closely adapted from the proven tester, to own PA11/PA12 OTG FS routing, the device-controller/FIFO configuration, static USB class storage, and the OTG FS interrupt handler. The V1 PA9 divider cannot drive valid hardware VBUS detection, so the board-selected assume-present mode leaves PA9 untouched and disables sensing. A corrected board can select sense-input mode through the same hardware contract. All other deferred peripheral pins remain untouched. See `docs/flightcomputer-v1-hardware.md` for the reviewed physical map and unresolved choices.

`rgb_led.c` owns the WS2812 electrical protocol. It preloads PA1 low before
selecting push-pull output mode and uses the tester-proven 168 MHz DWT timing,
GRB byte order, and MSB-first encoding. The application requests yellow while
startup calibration is in progress, green while disarmed, and off before arm
preparation. Noncritical colour changes are drained by a 10 Hz background task
so they cannot delay a disarm stop frame.
