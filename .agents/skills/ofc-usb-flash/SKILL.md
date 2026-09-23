---
name: ofc-usb-flash
description: Build, update, and verify this project's flight firmware through its resident USB-C bootloader when the user asks to flash the FC over USB. Do not use for SWD flashing or unrelated firmware work.
---

# OpenFlightComputer USB Flash

Run the existing host workflow from the `flight-computer-firmware` repository
root. It already builds the application, validates its flash region, requests a
disarmed bootloader handoff, transfers acknowledged chunks, verifies the CRC,
reboots, and checks the running build ID.

## Safety gate

Require confirmation that the flight battery is disconnected before starting.
An explicit confirmation in the current conversation is sufficient; do not ask
twice. This remains required with propellers removed because flashing resets
the motor-control application.

## Normal workflow

Use Release unless the user explicitly requests Debug. Make one tool call:

```bash
UV_CACHE_DIR=/private/tmp/ofc-uv-cache ./ofc firmware flash-usb --profile release
```

Request USB-device escalation for that call with the reusable approval prefix
`["./ofc", "firmware", "flash-usb"]`. Do not run separate build, device-status,
port-listing, or bootloader commands first; the workflow already owns those
steps.

Declare success only when the command exits successfully and reports `USB
update verified by the running firmware`. Report the flashed image, device,
and build ID. If the build ID ends in `-dirty`, explain the relevant
uncommitted source changes; do not commit or discard changes merely to remove
the suffix.

## Failure handling

- A timeout before `Streaming application` means no image transfer began. Ask
  the user to unplug and reconnect USB-C while keeping the battery disconnected,
  then rerun the same command once. Avoid status probes and automatic retry
  loops.
- If transfer began but was interrupted, rerun the same workflow; it can attach
  directly to the resident loader and recover the application.
- If the verified application does not return, report the last completed stage
  and retain SWD as the recovery path. Do not switch to SWD unless requested.

Do not arm, run motors, alter configuration, commit, or push as part of this
skill.
