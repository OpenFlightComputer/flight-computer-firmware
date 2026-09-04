# Flight Computer V1 board support

This board implementation targets manufacturing revision 1.7, generated from schematic revision 0.1, using an STM32F405RGT6 and a 16 MHz HSE crystal.

`board.c` owns startup policy: initialize the selected MCU, require successful clock configuration, verify the expected 168 MHz system clock, and start the selected timebase. `board_definition.h` records stable board identity and clock facts plus the V1 choice of an 84 MHz TIM5 input, a 1 MHz counter, interrupt priority zero, a 168 MHz TIM8 input, and assume-present USB VBUS behavior.

`motor_output_map.*` records the fixed `ESC_M1` through `ESC_M4` routes on
PC9 through PC6 as reversed TIM8 channels 4 through 1, and the selected grouped
TIM8-update DMA2 Stream 1/Channel 7 burst into CCR1 through CCR4. It is data
only: no motor GPIO, timer, or DMA resource is initialized by this milestone.

`time.c` exposes the generic `time_us()` API without leaking the STM32 backend. Milestone 0.10 adds `usb_device_port.c`, closely adapted from the proven tester, to own PA11/PA12 OTG FS routing, the device-controller/FIFO configuration, static USB class storage, and the OTG FS interrupt handler. The V1 PA9 divider cannot drive valid hardware VBUS detection, so the board-selected assume-present mode leaves PA9 untouched and disables sensing. A corrected board can select sense-input mode through the same hardware contract. All other deferred peripheral pins remain untouched. See `docs/flightcomputer-v1-hardware.md` for the reviewed physical map and unresolved choices.

`rgb_led_safe_state.c` owns only the WS2812's deterministic boot-off behavior.
It preloads PA1 low before selecting push-pull output mode, sends one valid
all-zero GRB frame using the tester's physically accepted DWT timing, and
leaves the pin low. This clears a colour retained across an MCU reset and
prevents later floating-input noise. It does not make the RGB LED a status
indicator or provide a general colour-output API.
