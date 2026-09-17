import struct
import zlib
from pathlib import Path

import pytest

from openflightcomputer.models import ProgressEvent
from openflightcomputer.programmer import ProgrammingError
from openflightcomputer.workflows.usb_flash import (
    _stream_application,
    application_payload_from_elf,
    validate_application_elf,
)


def _elf(segment_address: int, payload: bytes = b"test") -> bytes:
    identification = b"\x7fELF\x01\x01\x01" + bytes(9)
    header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        identification, 2, 40, 1, segment_address | 1, 52, 0, 0,
        52, 32, 1, 0, 0, 0,
    )
    program = struct.pack(
        "<IIIIIIII", 1, 84, segment_address, segment_address,
        len(payload), len(payload), 5, 4
    )
    return header + program + payload


def test_usb_flash_accepts_application_only_elf(tmp_path: Path):
    image = tmp_path / "application.elf"
    image.write_bytes(_elf(0x08010000))
    validate_application_elf(image)
    assert application_payload_from_elf(image) == b"test"


def test_usb_flash_rejects_configuration_sector_elf(tmp_path: Path):
    image = tmp_path / "configuration.elf"
    image.write_bytes(_elf(0x080E0000))
    with pytest.raises(ProgrammingError, match="relocated application partition"):
        validate_application_elf(image)


class FakeBootloaderConnection:
    def __init__(self) -> None:
        self.responses = [b"OFCBOOT 1 VBUS_ASSUME"]
        self.writes: list[bytes] = []

    def write_line(self, payload: bytes, *, timeout_seconds: float) -> None:
        del timeout_seconds
        self.writes.append(payload)
        if payload.startswith(b"BEGIN "):
            self.responses.append(b"READY")
        elif payload.startswith(b"DATA "):
            fields = payload.split(b" ", 2)
            chunk_length = len(bytes.fromhex(fields[2].decode("ascii")))
            self.responses.append(
                f"ACK {int(fields[1]) + chunk_length}".encode("ascii")
            )
        elif payload == b"END":
            self.responses.append(b"OK")
        elif payload == b"BOOT":
            self.responses.append(b"BOOTING")

    def read_line(self, *, timeout_seconds: float) -> bytes:
        del timeout_seconds
        return self.responses.pop(0)


def test_stream_application_sends_crc_sequential_chunks_and_boots():
    connection = FakeBootloaderConnection()
    payload = bytes(index % 251 for index in range(1300))
    progress: list[ProgressEvent] = []

    _stream_application(
        connection,
        payload,
        timeout_seconds=2.0,
        progress=progress.append,
    )

    assert connection.writes[0] == b"INFO"
    assert connection.writes[1] == (
        f"BEGIN 1300 {zlib.crc32(payload):08X}".encode("ascii")
    )
    data_lines = [line for line in connection.writes if line.startswith(b"DATA ")]
    assert [int(line.split(b" ", 2)[1]) for line in data_lines] == [0, 512, 1024]
    assert connection.writes[-2:] == [b"END", b"BOOT"]
    assert progress[-1].message == "Transferred 1300 of 1300 bytes"
