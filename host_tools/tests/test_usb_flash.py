import struct
from pathlib import Path

import pytest

from openflightcomputer.programmer import ProgrammingError
from openflightcomputer.workflows.usb_flash import validate_application_elf


def _elf(segment_address: int) -> bytes:
    identification = b"\x7fELF\x01\x01\x01" + bytes(9)
    header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        identification, 2, 40, 1, 0x08000001, 52, 0, 0,
        52, 32, 1, 0, 0, 0,
    )
    program = struct.pack(
        "<IIIIIIII", 1, 84, segment_address, segment_address, 4, 4, 5, 4
    )
    return header + program + b"test"


def test_usb_flash_accepts_application_only_elf(tmp_path: Path):
    image = tmp_path / "application.elf"
    image.write_bytes(_elf(0x08000000))
    validate_application_elf(image)


def test_usb_flash_rejects_configuration_sector_elf(tmp_path: Path):
    image = tmp_path / "configuration.elf"
    image.write_bytes(_elf(0x080E0000))
    with pytest.raises(ProgrammingError, match="configuration sector"):
        validate_application_elf(image)
