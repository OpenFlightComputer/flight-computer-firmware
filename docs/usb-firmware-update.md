# USB firmware updates

OpenFlightComputer uses a small resident bootloader for routine USB-C updates.
The same bootloader binary supports both existing V1 hardware with unusable
VBUS sensing and a later board with corrected VBUS sensing. SWD remains the
independent first-install, debug, and recovery path.

## Flash layout

| Region | Address | Purpose |
| --- | --- | --- |
| Resident bootloader | `0x08000000`–`0x0800FFFF` | Protected by update policy; installed through SWD |
| Flight application | `0x08010000`–`0x080DFEFF` | Replaceable through USB or SWD |
| Application metadata | `0x080DFF00`–`0x080DFFFF` | Length and CRC committed after a complete update |
| Flight configuration | `0x080E0000`–`0x080FFFFF` | Preserved by both update paths |

The metadata magic is written last. Until that final commit, the bootloader
considers the application invalid and does not jump to it. A reset or cable
loss during erase, transfer, or verification therefore returns to the USB
bootloader instead of attempting to execute a partial image.

## First installation and SWD recovery

```bash
./ofc firmware flash --profile release
```

This command builds and installs three artifacts through the connected
ST-Link: the resident bootloader, the relocated application, and the matching
application metadata. It then resets the target. An existing board that still
contains the old application at `0x08000000` needs this one SWD installation
before it can update through USB.

SWD does not depend on either bootloader or USB. It remains able to replace a
damaged bootloader and is therefore the authoritative recovery path.

## Routine USB update

```bash
./ofc firmware flash-usb --profile release
```

The host tool:

1. builds the relocated application unless `--firmware IMAGE.elf` is supplied;
2. validates that all file-backed ELF segments lie in the application region;
3. asks a running, disarmed application to enter its resident bootloader, or
   attaches directly when the bootloader is already waiting after a failed
   update;
4. sends the exact image length and CRC, then sequential bounded data chunks;
5. requires an acknowledgement for every chunk;
6. asks the bootloader to verify flash, commit metadata, and reboot; and
7. reconnects to the flight application and checks its build ID when known.

The loader only erases application sectors 4 through 10. It cannot erase its
own sectors 0 through 3 and does not touch configuration sector 11.

## Runtime VBUS selection

There are not separate V1 and V2 firmware builds. On every boot, the board
layer samples PA9 before USB initialization:

- a valid high level selects hardware VBUS sensing;
- a low or electrically invalid level selects the assume-present fallback.

The existing V1 divider falls into the fallback path, so the custom loader can
enumerate without relying on its faulty sensing voltage. A corrected board
with a valid PA9 VBUS signal automatically uses normal sensing. The selected
mode changes only the USB peripheral setup; update validation and flash layout
are identical.

## Application handoff and safety

`bootloader_enter` is accepted only while safely disarmed and with no active
or pending motor owner. The application requests a physical motor stop, waits
until the complete command acknowledgement leaves the CDC queue, writes a
one-shot request marker to an RTC backup register, and resets.

At reset the resident bootloader consumes that marker. With no request, it
checks metadata, vector-table addresses, image length, and image CRC before
jumping to `0x08010000`. With a request or invalid application it exposes the
bootloader CDC identity `CAFE:4003`; the flight application uses `CAFE:4002`.

The application validity check proves integrity, not authenticity. This is a
physically trusted development update interface; signed production images and
read/write protection are separate future security work.
