# Flight Computer V1 board support

This board implementation targets manufacturing revision 1.7, generated from schematic revision 0.1, using an STM32F405RGT6 and a 16 MHz HSE crystal.

`board.c` owns startup policy: initialize the selected MCU, require successful clock configuration, and verify the expected 168 MHz system clock. `board_definition.h` records stable board identity and clock facts. It deliberately does not define unused GPIO objects for future peripherals.

No peripheral pins are initialized in Milestone 0.3. See `docs/flightcomputer-v1-hardware.md` for the reviewed physical map and unresolved choices.
