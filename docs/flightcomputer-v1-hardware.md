# Flight Computer V1 hardware map

This document records the Phase 0, Milestone 0.3 review of the authoritative Flight Computer V1 design. It is a firmware reference, not a replacement for KiCad.

## Reviewed source snapshot

- Hardware repository: `OpenFlightComputer/flight-computer-hardware`
- Branch and commit: `main`, `e1f4bdebb0fe18a274fba08a50c99ed978c7ec72`
- Working state: owner modifications and untracked manufacturing files were present; firmware work did not modify them
- Root project: `FlightComputer_V1.kicad_pro`
- Root schematic: `FlightComputer_V1.kicad_sch`
- PCB: `FlightComputer_V1.kicad_pcb`
- Manufacturing revision: `1.7`
- Schematic title/revision: `First PCB`, revision `0.1`

The files still matched the snapshot inspected by the manufacturing-test project:

| File | SHA-256 |
| --- | --- |
| `FlightComputer_V1.kicad_pro` | `ed197e094f1406852b88b31015f0e1152ceef05802e561fce1d1354021c72d44` |
| `FlightComputer_V1.kicad_sch` | `90c675907b837f97d827d3e5cbc24ff5cf763a53324cc96fd155d12ba1f80347` |
| `FlightComputer_V1.kicad_pcb` | `3f0a6b184265de4666bb49451353d1cc9ca22d569f1a510fe64b1ad4d7d17f23` |

The review also cross-checked these public manufacturing-test sources:

- `docs/hardware-reference.md`
- `configs/board/flightcomputer-v1.json`
- `firmware/manufacturing_test/board_support/flightcomputer_v1/`
- `firmware/manufacturing_test/components/status_leds/status_led_test.c`
- `firmware/manufacturing_test/components/rgb_led/rgb_led_test.c`

## Installed hardware

| Reference | Part/value | Firmware role |
| --- | --- | --- |
| U5 | STM32F405RGT6 (`STM32F405RGTx` in the schematic) | Main MCU, LQFP-64 |
| U1 | BMI270 | SPI IMU with two routed interrupt signals |
| U3 | BMP388 | I2C barometer; CSB and SDO straps select I2C/address behavior |
| Card1 | XKTF-015-N | microSD socket in SPI mode with card detect |
| USBC1 | TYPE-C-31-M-12 | USB-C power and USB 2.0 Full-Speed data |
| J5 | 1x5 connector | 3.3 V reference, SWDIO, SWCLK, NRST, and GND |
| D4, D5 | Red and green LEDs | Discrete status indicators |
| LED1 | WS2812B-B/W | 5 V addressable RGB LED driven by a 3.3 V MCU signal |
| Y1 | 7B016000F01 | 16 MHz HSE crystal |

## MCU signal map

Directions are from the MCU perspective. A selected peripheral indicates either an unambiguous routed function or an existing manufacturing-test selection. A candidate remains uncommitted flight-firmware work.

| MCU pin | Net | Direction | Semantic role | Peripheral/mode | Selection status |
| --- | --- | --- | --- | --- | --- |
| PA1 | `WS2812_DI` | Output | RGB LED data | TIM2_CH2 AF1; DMA1 Stream 6 Channel 3 | Implemented by tester; not initialized here |
| PA2 | `GPS_RX` | Output | MCU-to-GPS serial | USART2_TX AF7 | Unambiguous intended UART function |
| PA3 | `GPS_TX` | Input | GPS-to-MCU serial | USART2_RX AF7 | Unambiguous intended UART function |
| PA4 | `VBAT_ADC` | Analog input | Battery-voltage sense | ADC1_IN4 | Routed; conversion policy deferred |
| PA5 | `SD_SCK` | Output | microSD SPI clock | SPI1_SCK AF5 | Implemented by tester; not initialized here |
| PA6 | `SD_MISO` | Input | microSD data to MCU | SPI1_MISO AF5 | Implemented by tester; not initialized here |
| PA7 | `SD_MOSI` | Output | microSD data from MCU | SPI1_MOSI AF5 | Implemented by tester; not initialized here |
| PA9 | unnamed VBUS divider net | Input | USB VBUS sense | OTG_FS_VBUS | Implemented by tester; not initialized here |
| PA11 | `USB_DN` | Bidirectional | USB data minus | OTG_FS_DM AF10 | Implemented by tester; not initialized here |
| PA12 | `USB_DP` | Bidirectional | USB data plus | OTG_FS_DP AF10 | Implemented by tester; not initialized here |
| PA13 | `SWDIO` | Bidirectional | SWD data | JTMS/SWDIO AF0 | Fixed debug function |
| PA14 | `SWCLK` | Input | SWD clock | JTCK/SWCLK AF0 | Fixed debug function |
| PB0 | `CURR_ADC` | Analog input | ESC current sense | ADC1_IN8 | Routed; conversion policy deferred |
| PB3 | `IMU_SPI_SCK` | Output | BMI270 SPI clock | SPI3_SCK AF6 | Selected and implemented by tester |
| PB4 | `IMU_SPI_MISO` | Input | BMI270 data to MCU | SPI3_MISO AF6 | Selected and implemented by tester |
| PB5 | `IMU_SPI_MOSI` | Output | BMI270 data from MCU | SPI3_MOSI AF6 | Selected and implemented by tester |
| PB6 | `IMU_INT2` | Input | BMI270 interrupt 2 | GPIO/EXTI | EXTI configuration deferred |
| PB9 | `GPS_PPS` | Input | GPS pulse-per-second | GPIO/EXTI or timer capture | Selection unresolved |
| PB10 | `I2C_SCL` | Bidirectional open-drain | BMP388 clock | I2C2_SCL AF4 | Implemented by tester; not initialized here |
| PB11 | `I2C_SDA` | Bidirectional open-drain | BMP388 data | I2C2_SDA AF4 | Implemented by tester; not initialized here |
| PB13 | `LED_RED` | Output | Red status LED | GPIO | Implemented by tester; electrical concern remains |
| PB14 | `LED_GREEN` | Output | Green status LED | GPIO | Implemented by tester; electrical concern remains |
| PC4 | `SD_CS` | Output | microSD chip select | GPIO | Implemented by tester; not initialized here |
| PC5 | `SD_DET` | Input | microSD card detect | GPIO/EXTI | Tester provisionally treats as active-low |
| PC6 | `ESC_M4` | Output | ESC/motor channel 4 | TIM8_CH1 AF3 candidate | Timer/DMA selection deferred to Phase 1 |
| PC7 | `ESC_M3` | Output | ESC/motor channel 3 | TIM8_CH2 AF3 candidate | Timer/DMA selection deferred to Phase 1 |
| PC8 | `ESC_M2` | Output | ESC/motor channel 2 | TIM8_CH3 AF3 candidate | Timer/DMA selection deferred to Phase 1 |
| PC9 | `ESC_M1` | Output | ESC/motor channel 1 | TIM8_CH4 AF3 candidate | Timer/DMA selection deferred to Phase 1 |
| PC10 | `RP1_RX` | Output | MCU-to-receiver/telemetry serial | UART4_TX AF8 or USART3_TX AF7 | UART selection unresolved |
| PC11 | `RP1_TX` | Input | Receiver/telemetry serial to MCU | UART4_RX AF8 or USART3_RX AF7 | UART selection unresolved |
| PC12 | `IMU_INT1` | Input | BMI270 interrupt 1 | GPIO/EXTI | EXTI configuration deferred |
| PD2 | `IMU_CS` | Output | BMI270 chip select | GPIO | Implemented by tester; not initialized here |
| PH0 | `HSE_IN` | Input | External high-speed clock | OSC_IN | Active in Milestone 0.2/0.3 clock setup |
| PH1 | `HSE_OUT` | Output | External high-speed clock | OSC_OUT | Active in Milestone 0.2/0.3 clock setup |
| NRST | `NRST` | Input/open-drain | Reset and SWD recovery | Reset function | Fixed function |
| BOOT0 | `BOOT0` | Input | Boot selection | Boot strap | Pulled down with manual switch |

Names such as `GPS_RX` and `RP1_RX` are from the external-device perspective. The direction and semantic-role columns above state the MCU perspective explicitly.

## Resolved interface selections

Most selections below are established by routing plus the manufacturing-test implementation. The internal timebase selection was added by Milestone 0.4 after checking it does not conflict with those established timer uses.

| Interface | Selection | Relevant V1 signals |
| --- | --- | --- |
| System clock | 16 MHz HSE; PLLM 16, PLLN 336, PLLP 2, PLLQ 7 | PH0, PH1 |
| Monotonic timebase | TIM5, internal 1 MHz free-running counter | No routed pin or DMA stream |
| USB | OTG FS AF10 with VBUS sensing | PA9, PA11, PA12 |
| microSD | SPI1 AF5 plus GPIO chip select/detect | PA5, PA6, PA7, PC4, PC5 |
| BMI270 | SPI3 AF6 plus GPIO chip select | PB3, PB4, PB5, PD2 |
| BMP388 | I2C2 AF4 | PB10, PB11 |
| GPS serial | USART2 AF7 | PA2, PA3 |
| WS2812 | TIM2_CH2 AF1; DMA1 Stream 6 Channel 3 | PA1 |
| SWD | STM32 fixed SWD functions plus NRST | PA13, PA14, NRST |

## Deliberately unresolved decisions

- PC10/PC11 can map to UART4 or USART3. Phase 2 will select the receiver backend after checking protocol, DMA, interrupt, and other UART requirements.
- PC6–PC9 form a natural TIM8 channel group, but the DShot rate, timer setup, DMA mapping, and synchronization strategy belong to Phase 1 Milestones 1.4–1.5.
- PB9 may use a normal EXTI input or timer capture for GPS PPS. Phase 6 will choose based on timing requirements.
- BMI270 interrupt routing and EXTI selection belong to the sensor timing milestone.
- ADC sample timing and scaling for PA4/PB0 are not yet specified.

## Hardware concerns retained for validation

- D4/D5 appear reversed relative to the standard KiCad LED symbol: their annotated anodes connect toward ground and cathodes toward the MCU through resistors. Treat status LED polarity and operability as unresolved electrical behavior.
- The WS2812 is powered from 5 V and driven directly by 3.3 V. Its fitted-device threshold appears compatible, but waveform and noise margin still require physical validation.
- PC5 card detect is provisionally active-low from the socket and pull-up topology; confirm with the physical board.
- The hardware working tree contains owner changes. The hashes above identify the exact reviewed design files without treating the Git commit alone as complete provenance.

## Milestone 0.4 initialization scope

At boot, `board_initialize()` initializes the STM32 HAL and system clock, verifies a 168 MHz core clock, and starts internal TIM5 as the monotonic timebase. It does not configure any GPIO or routed peripheral listed above. Each external interface will be activated only in its approved milestone through the appropriate board and MCU capability boundary.
