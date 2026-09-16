from pathlib import Path

import pytest

from openflightcomputer.models import Probe
from openflightcomputer.external_tools import CommandResult
from openflightcomputer.programmer import (
    ProgrammingError,
    Stm32CubeProgrammer,
    flash_firmware,
)


class FakeProgrammer:
    def __init__(self, probes):
        self.probes = probes
        self.operations = []

    def discover_probes(self):
        return self.probes

    def program_and_verify(self, probe, path):
        self.operations.append(("program", probe, path))

    def reset(self, probe):
        self.operations.append(("reset", probe))


def test_flash_selects_programs_verifies_and_resets(tmp_path: Path):
    image = tmp_path / "flight.elf"
    image.write_bytes(b"elf")
    programmer = FakeProgrammer((Probe("one"),))
    assert flash_firmware(programmer, image).serial_number == "one"
    assert [operation[0] for operation in programmer.operations] == ["program", "reset"]


def test_multiple_probes_require_selection(tmp_path: Path):
    image = tmp_path / "flight.elf"
    image.write_bytes(b"elf")
    programmer = FakeProgrammer((Probe("one"), Probe("two")))
    with pytest.raises(ProgrammingError, match="multiple"):
        flash_firmware(programmer, image)


def test_cube_programmer_discovers_dfu_and_programs_without_mass_erase(
    tmp_path: Path,
):
    image = tmp_path / "flight.elf"
    image.write_bytes(b"elf")
    calls = []

    def run(arguments, *, cwd, timeout_seconds):
        calls.append(tuple(arguments))
        if tuple(arguments[-2:]) == ("-l", "usb"):
            return CommandResult(
                tuple(arguments), 0,
                "\x1b[32mDevice Index : USB1\x1b[0m\n", "",
            )
        return CommandResult(tuple(arguments), 0, "verified", "")

    programmer = Stm32CubeProgrammer(Path("/tool"), command_runner=run)
    assert programmer.discover_dfu_ports() == ("USB1",)
    programmer.program_dfu_and_start("USB1", image)
    assert calls[-1] == (
        "/tool", "-c", "port=USB1", "-d", str(image),
        "-v", "-s", "0x08000000",
    )
    assert "-e" not in calls[-1]
