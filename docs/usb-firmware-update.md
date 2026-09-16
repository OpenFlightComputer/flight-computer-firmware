# USB firmware updates

The V1 flight computer can update its application through the existing USB-C
connector by using the STM32F405 factory ROM DFU bootloader. No project-owned
bootloader occupies application flash.

## Normal workflow

```bash
./ofc firmware flash-usb --profile release
```

The host tool:

1. builds the firmware, unless `--firmware IMAGE.elf` was supplied;
2. verifies that every file-backed ELF load segment lies in application flash
   `0x08000000` through `0x080DFFFF`;
3. connects to the running firmware and sends `bootloader_enter`;
4. waits for the STM32 factory DFU interface;
5. downloads and verifies the ELF without mass erase, then starts address
   `0x08000000`;
6. waits for flight-firmware USB CDC and confirms the running build ID.

The configuration partition begins at `0x080E0000` and is preserved. A
supplied ELF has no generated host metadata, so status is still read after the
update but exact build-ID comparison is only available for an image built by
the same command.

## Firmware safety contract

`bootloader_enter` is accepted only in `DISARMED`, with no active or pending
motor-command owner. Firmware requests a physical motor-output stop before it
acknowledges the command. The USB service does not reset until the complete
acknowledgement has left the CDC transmit queue.

The handoff stores a one-shot magic value in an RTC backup register and issues
a system reset. At the beginning of `main`, before HAL or application
initialization, the board layer consumes and clears that marker, disables
interrupt state, remaps STM32 system memory, and jumps to the ROM reset vector.
Because the marker is cleared before the jump, a later reset boots the normal
application even if the DFU update was interrupted.

This development interface is physically trusted and unauthenticated. It is
not a remote update or production security mechanism.

## Recovery

The ROM handoff requires a working application and USB command path. If either
is damaged, flash over SWD:

```bash
./ofc firmware flash --profile release
```

SWD remains the authoritative recovery and debugging interface.

## Device basis

ST's current AN2606 lists STM32F40xxx/41xxx ROM DFU on USB OTG FS PA11/PA12,
with an external clock in whole-MHz steps from 4 through 26 MHz. V1's 16 MHz
HSE is within that range. The STM32F405 table uses PA9 for USART1 TX rather
than listing it as USB VBUS, so this path does not depend on the V1
application's configurable VBUS-sensing workaround. Physical enumeration on
the assembled V1 board is still required before this becomes the routine
flashing method.

- [AN2606: STM32 system-memory boot mode](https://www.st.com/resource/en/application_note/an2606-introduction-to-system-memory-boot-mode-on-stm32-mcus-stmicroelectronics.pdf)
- [AN3156: USB DFU protocol used in STM32 bootloaders](https://www.st.com/resource/en/application_note/an3156-how-to-use-usb-dfu-protocol-in-bootloader-on-stm32-mcus-stmicroelectronics.pdf)
