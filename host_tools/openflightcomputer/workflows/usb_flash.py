"""Safe application update through the resident OpenFlightComputer bootloader."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

from openflightcomputer.device import (
    BOOTLOADER_USB_PID,
    FLIGHT_USB_VID,
    UsbCdcConnection,
    list_available_ports,
    wait_for_bootloader_port,
    wait_for_flight_port,
)
from openflightcomputer.firmware import ProgressCallback, build_firmware
from openflightcomputer.models import (
    FirmwareArtifact,
    FirmwareProfile,
    ProgressEvent,
    UsbFlashOutcome,
)
from openflightcomputer.programmer import ProgrammingError
from openflightcomputer.protocol import JsonProtocolClient, ProtocolError


APPLICATION_START = 0x08010000
APPLICATION_END = 0x080DFF00
PT_LOAD = 1
EM_ARM = 40
TRANSFER_CHUNK_BYTES = 512


def application_payload_from_elf(path: Path) -> bytes:
    """Validate an application-only ELF and return its contiguous flash bytes."""
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
    segments: list[tuple[int, bytes]] = []
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
        if not (APPLICATION_START <= physical_address < end <= APPLICATION_END):
            raise ProgrammingError(
                "firmware ELF would write outside the relocated application partition"
            )
        segments.append((physical_address, data[file_offset:file_offset + file_size]))
    if not segments or min(address for address, _payload in segments) != APPLICATION_START:
        raise ProgrammingError(
            f"firmware ELF does not contain an application at 0x{APPLICATION_START:08X}"
        )
    entry_address = entry & ~1
    if not any(
        address <= entry_address < address + len(payload)
        for address, payload in segments
    ):
        raise ProgrammingError("firmware ELF entry point is outside its application image")
    highest = max(address + len(payload) for address, payload in segments)
    image = bytearray(b"\xff" * (highest - APPLICATION_START))
    for address, payload in segments:
        start = address - APPLICATION_START
        image[start:start + len(payload)] = payload
    return bytes(image)


def validate_application_elf(path: Path) -> None:
    application_payload_from_elf(path)


def _bootloader_is_present() -> bool:
    return any(
        port.vid == FLIGHT_USB_VID and port.pid == BOOTLOADER_USB_PID
        for port in list_available_ports()
    )


def _request_bootloader(
    requested_port: str | Path | None, timeout_seconds: float
) -> None:
    flight_port = wait_for_flight_port(
        requested_port, timeout_seconds=timeout_seconds
    )
    with UsbCdcConnection.open(flight_port) as connection:
        response = JsonProtocolClient(connection).request(
            "bootloader_enter", timeout_seconds=timeout_seconds
        )
    if response.get("state") != "DISARMED":
        raise ProtocolError("bootloader handoff was not acknowledged while disarmed")


def _expect(connection: UsbCdcConnection, expected: bytes, timeout: float) -> None:
    received = connection.read_line(timeout_seconds=timeout)
    if received != expected:
        raise ProtocolError(
            f"bootloader returned {received!r}; expected {expected!r}"
        )


def _stream_application(
    connection: UsbCdcConnection,
    payload: bytes,
    *,
    timeout_seconds: float,
    progress: ProgressCallback,
) -> None:
    checksum = zlib.crc32(payload)
    connection.write_line(b"INFO", timeout_seconds=timeout_seconds)
    info = connection.read_line(timeout_seconds=timeout_seconds)
    if not info.startswith(b"OFCBOOT 1 "):
        raise ProtocolError(f"unsupported bootloader response: {info!r}")
    connection.write_line(
        f"BEGIN {len(payload)} {checksum:08X}".encode("ascii"),
        timeout_seconds=timeout_seconds,
    )
    _expect(connection, b"READY", max(timeout_seconds, 60.0))
    offset = 0
    while offset < len(payload):
        chunk = payload[offset:offset + TRANSFER_CHUNK_BYTES]
        connection.write_line(
            f"DATA {offset} ".encode("ascii") + chunk.hex().encode("ascii"),
            timeout_seconds=timeout_seconds,
        )
        offset += len(chunk)
        _expect(connection, f"ACK {offset}".encode("ascii"), timeout_seconds)
        if offset == len(payload) or offset % (32 * 1024) == 0:
            progress(ProgressEvent(
                "usb-flash", f"Transferred {offset} of {len(payload)} bytes"
            ))
    connection.write_line(b"END", timeout_seconds=timeout_seconds)
    _expect(connection, b"OK", max(timeout_seconds, 30.0))
    connection.write_line(b"BOOT", timeout_seconds=timeout_seconds)
    _expect(connection, b"BOOTING", timeout_seconds)


def build_and_flash_usb(
    profile: FirmwareProfile = "release",
    *,
    firmware_path: Path | None = None,
    programmer_path: Path | None = None,
    requested_port: str | Path | None = None,
    timeout_seconds: float = 20.0,
    progress: ProgressCallback = lambda _event: None,
) -> UsbFlashOutcome:
    del programmer_path  # Retained for compatibility with older CLI invocations.
    artifact = (
        build_firmware(profile, progress=progress)
        if firmware_path is None
        else FirmwareArtifact(
            profile=profile,
            elf_path=firmware_path.expanduser().resolve(),
        )
    )
    payload = application_payload_from_elf(artifact.elf_path)
    if not _bootloader_is_present():
        progress(ProgressEvent("usb-flash", "Requesting disarmed bootloader handoff"))
        _request_bootloader(requested_port, timeout_seconds)
    progress(ProgressEvent("usb-flash", "Waiting for resident USB bootloader"))
    bootloader_port = wait_for_bootloader_port(None, timeout_seconds=timeout_seconds)
    progress(ProgressEvent(
        "usb-flash", f"Streaming application through {bootloader_port.device}"
    ))
    with UsbCdcConnection.open(bootloader_port) as connection:
        _stream_application(
            connection,
            payload,
            timeout_seconds=timeout_seconds,
            progress=progress,
        )
    progress(ProgressEvent("usb-flash", "Waiting for updated application"))
    running_port = wait_for_flight_port(None, timeout_seconds=timeout_seconds)
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
        bootloader_port=bootloader_port.device,
        device_port=running_port.device,
        status=status,
    )
