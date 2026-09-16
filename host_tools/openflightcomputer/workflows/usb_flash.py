"""Safe application-only update through the STM32 factory USB DFU loader."""

from __future__ import annotations

import struct
import time
from collections.abc import Callable
from pathlib import Path

from openflightcomputer.device import UsbCdcConnection, wait_for_flight_port
from openflightcomputer.firmware import ProgressCallback, build_firmware
from openflightcomputer.models import (
    FirmwareArtifact,
    FirmwareProfile,
    ProgressEvent,
    UsbFlashOutcome,
)
from openflightcomputer.programmer import ProgrammingError, Stm32CubeProgrammer
from openflightcomputer.protocol import JsonProtocolClient, ProtocolError


APPLICATION_START = 0x08000000
CONFIGURATION_START = 0x080E0000
PT_LOAD = 1
EM_ARM = 40


def validate_application_elf(path: Path) -> None:
    """Reject images that could write outside the application flash partition."""
    try:
        data = path.read_bytes()
    except OSError as error:
        raise ProgrammingError(f"cannot read firmware ELF: {error}") from error
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4:6] != b"\x01\x01":
        raise ProgrammingError("USB flashing requires a 32-bit little-endian ELF image")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    machine, entry, phoff, phentsize, phnum = (
        header[2], header[4], header[5], header[9], header[10]
    )
    if machine != EM_ARM or phentsize < 32:
        raise ProgrammingError("USB flashing requires an ARM ELF32 image")
    ranges: list[tuple[int, int]] = []
    for index in range(phnum):
        offset = phoff + index * phentsize
        if offset + 32 > len(data):
            raise ProgrammingError("firmware ELF has a truncated program header table")
        segment = struct.unpack_from("<IIIIIIII", data, offset)
        segment_type, file_offset, physical_address, file_size = (
            segment[0], segment[1], segment[3], segment[4]
        )
        if segment_type != PT_LOAD or file_size == 0:
            continue
        if file_offset + file_size > len(data):
            raise ProgrammingError("firmware ELF has a truncated load segment")
        end = physical_address + file_size
        if not (APPLICATION_START <= physical_address < end <= CONFIGURATION_START):
            raise ProgrammingError(
                "firmware ELF would write outside application flash; "
                "the persistent configuration sector was not touched"
            )
        ranges.append((physical_address, end))
    if not ranges or min(start for start, _end in ranges) != APPLICATION_START:
        raise ProgrammingError(
            "firmware ELF does not contain an application at 0x08000000"
        )
    entry_address = entry & ~1
    if not any(start <= entry_address < end for start, end in ranges):
        raise ProgrammingError("firmware ELF entry point is outside its application image")


def _wait_for_one_dfu_port(
    programmer: Stm32CubeProgrammer,
    timeout_seconds: float,
    *,
    monotonic: Callable[[], float] = time.monotonic,
    sleeper: Callable[[float], None] = time.sleep,
) -> str:
    deadline = monotonic() + timeout_seconds
    while True:
        ports = programmer.discover_dfu_ports()
        if len(ports) == 1:
            return ports[0]
        if len(ports) > 1:
            raise ProgrammingError(
                "multiple STM32 DFU devices found; disconnect all but the target"
            )
        now = monotonic()
        if now >= deadline:
            raise ProgrammingError(
                f"STM32 USB DFU device did not appear within {timeout_seconds:g}s"
            )
        sleeper(min(0.25, deadline - now))


def build_and_flash_usb(
    profile: FirmwareProfile = "release",
    *,
    firmware_path: Path | None = None,
    programmer_path: Path | None = None,
    requested_port: str | Path | None = None,
    timeout_seconds: float = 20.0,
    progress: ProgressCallback = lambda _event: None,
) -> UsbFlashOutcome:
    artifact = (
        build_firmware(profile, progress=progress)
        if firmware_path is None
        else FirmwareArtifact(
            profile=profile,
            elf_path=firmware_path.expanduser().resolve(),
        )
    )
    validate_application_elf(artifact.elf_path)
    programmer = Stm32CubeProgrammer.create(programmer_path)
    if programmer.discover_dfu_ports():
        raise ProgrammingError(
            "an STM32 DFU device is already connected; disconnect it before "
            "requesting the flight computer update"
        )

    progress(ProgressEvent("usb-flash", "Requesting disarmed ROM DFU handoff"))
    flight_port = wait_for_flight_port(
        requested_port, timeout_seconds=timeout_seconds
    )
    with UsbCdcConnection.open(flight_port) as connection:
        response = JsonProtocolClient(connection).request(
            "bootloader_enter", timeout_seconds=timeout_seconds
        )
    if response.get("state") != "DISARMED":
        raise ProtocolError("bootloader handoff was not acknowledged while disarmed")

    progress(ProgressEvent("usb-flash", "Waiting for STM32 factory USB DFU"))
    dfu_port = _wait_for_one_dfu_port(programmer, timeout_seconds)
    progress(ProgressEvent(
        "usb-flash", f"Programming and verifying through {dfu_port}"
    ))
    programmer.program_dfu_and_start(dfu_port, artifact.elf_path)

    progress(ProgressEvent("usb-flash", "Waiting for the updated application"))
    running_port = wait_for_flight_port(
        requested_port, timeout_seconds=timeout_seconds
    )
    with UsbCdcConnection.open(running_port) as connection:
        status = JsonProtocolClient(connection).request(
            "status", timeout_seconds=timeout_seconds
        )
    if artifact.build_id is not None and status.get("build_id") != artifact.build_id:
        raise ProtocolError(
            f"running build ID {status.get('build_id')!r} does not match "
            f"flashed build ID {artifact.build_id!r}"
        )
    progress(ProgressEvent(
        "usb-flash", "USB update verified by the running firmware"
    ))
    return UsbFlashOutcome(
        artifact=artifact,
        dfu_port=dfu_port,
        device_port=running_port.device,
        status=status,
    )
