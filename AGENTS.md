# Contributor guidance

This repository contains operational flight-control firmware for OpenFlightComputer. It does not contain KiCad sources, manufacturing-test policy, or the manufacturing-test application.

Work one approved milestone at a time. Read `DEVELOPMENT.md` before changing code, update it when a milestone ends, and stop for owner review before beginning the next milestone.

Keep these boundaries explicit:

- The `flight-computer-hardware` repository is the source of physical board truth.
- The `flight-computer-test` repository is the source of proven V1 bring-up knowledge and manufacturing-test implementations.
- Reuse verified low-level knowledge where appropriate, but do not copy the tester's application architecture into flight firmware.
- Never guess a routed pin, peripheral instance, alternate function, DMA stream, clock value, or electrical behavior.
- Preserve the dependency direction `app` -> `flight` -> `peripherals` -> `hardware/boards` -> `hardware/mcu`; hardware-independent logic must not accumulate STM32 HAL calls.
- Use pure C, fixed-capacity data structures, bounded execution, and no runtime dynamic allocation unless an exception is explicitly justified and approved.
- Keep interrupts short; substantial parsing, control, and logging work belongs outside interrupt context.
- Do not add DShot, CRSF/ELRS, sensor processing, stabilization, SD logging, simulation, an RTOS, a bootloader, or speculative MCU support before its approved milestone.

Run validation appropriate to the active milestone and distinguish host/build verification from physical-board verification. Record assumptions, unresolved questions, safety implications, and the next proposed milestone in `DEVELOPMENT.md`.

Always ask the project owner for approval immediately before creating or amending a Git commit.
