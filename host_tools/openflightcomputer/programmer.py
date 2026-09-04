"""Programmer-neutral flashing plus an STM32CubeProgrammer adapter."""

from __future__ import annotations

import os
import re
import shutil
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path
from typing import Protocol

from openflightcomputer.external_tools import (
    CommandRunner,
    ExternalCommandError,
    failure_detail,
    run_command,
)
from openflightcomputer.models import Probe, ProgressEvent


ProgressCallback = Callable[[ProgressEvent], None]
ExecutableLocator = Callable[[str], str | None]
_STLINK_SERIAL_PATTERN = re.compile(r"ST-LINK\s+SN\s*:\s*([0-9A-Za-z]+)", re.I)
_MACOS_APPLICATION = Path(
    "/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/"
    "STM32CubeProgrammer.app/Contents"
)
_APPLICATION_CANDIDATES = (
    _MACOS_APPLICATION / "MacOs/bin/STM32_Programmer_CLI",
    _MACOS_APPLICATION / "Resources/bin/STM32_Programmer_CLI",
)


class ProgrammingError(RuntimeError):
    pass


class Programmer(Protocol):
    def discover_probes(self) -> tuple[Probe, ...]: ...
    def program_and_verify(self, probe: Probe, firmware_path: Path) -> None: ...
    def reset(self, probe: Probe) -> None: ...


def locate_stm32cubeprogrammer(
    *,
    override: Path | None = None,
    executable_locator: ExecutableLocator = shutil.which,
    environment: Mapping[str, str] = os.environ,
    application_candidates: Sequence[Path] = _APPLICATION_CANDIDATES,
) -> Path:
    if override is not None:
        explicit = override.expanduser()
        if explicit.is_file() and os.access(explicit, os.X_OK):
            return explicit.resolve()
        raise ProgrammingError(f"programmer is not executable: {override}")
    candidates: list[Path] = []
    configured = environment.get("STM32CUBE_PROGRAMMER_CLI")
    if configured:
        candidates.append(Path(configured).expanduser())
    on_path = executable_locator("STM32_Programmer_CLI")
    if on_path:
        candidates.append(Path(on_path))
    candidates.extend(application_candidates)
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    raise ProgrammingError(
        "STM32CubeProgrammer CLI was not found; install it or set "
        "STM32CUBE_PROGRAMMER_CLI"
    )


class Stm32CubeProgrammer:
    def __init__(
        self,
        executable: Path,
        *,
        command_runner: CommandRunner = run_command,
        swd_frequency_khz: int = 1000,
    ) -> None:
        self._executable = executable
        self._run_command = command_runner
        self._frequency = swd_frequency_khz

    @classmethod
    def create(cls, executable: Path | None = None) -> "Stm32CubeProgrammer":
        return cls(locate_stm32cubeprogrammer(override=executable))

    def _run(self, arguments: tuple[str, ...], timeout_seconds: float):
        try:
            return self._run_command(arguments, cwd=None, timeout_seconds=timeout_seconds)
        except ExternalCommandError as error:
            raise ProgrammingError(str(error)) from error

    def discover_probes(self) -> tuple[Probe, ...]:
        result = self._run((str(self._executable), "--list"), 30)
        if result.returncode != 0:
            raise ProgrammingError("could not list ST-Link probes:\n" + failure_detail(result))
        return tuple(
            Probe(serial_number=serial)
            for serial in dict.fromkeys(_STLINK_SERIAL_PATTERN.findall(result.stdout))
        )

    def program_and_verify(self, probe: Probe, firmware_path: Path) -> None:
        result = self._run(
            (
                str(self._executable), "-c", "port=SWD",
                f"sn={probe.serial_number}", "mode=UR", f"freq={self._frequency}",
                "-d", str(firmware_path), "-v",
            ),
            120,
        )
        if result.returncode != 0:
            raise ProgrammingError("programming or verification failed:\n" + failure_detail(result))

    def reset(self, probe: Probe) -> None:
        result = self._run(
            (
                str(self._executable), "-c", "port=SWD",
                f"sn={probe.serial_number}", "mode=UR", f"freq={self._frequency}",
                "-rst",
            ),
            30,
        )
        if result.returncode != 0:
            raise ProgrammingError("firmware verified but reset failed:\n" + failure_detail(result))


def flash_firmware(
    programmer: Programmer,
    firmware_path: Path,
    *,
    requested_serial: str | None = None,
    progress: ProgressCallback = lambda _event: None,
) -> Probe:
    if not firmware_path.is_file():
        raise ProgrammingError(f"firmware ELF does not exist: {firmware_path}")
    probes = programmer.discover_probes()
    if requested_serial is not None:
        matches = tuple(p for p in probes if p.serial_number == requested_serial)
        if len(matches) != 1:
            available = ", ".join(p.serial_number for p in probes) or "none"
            raise ProgrammingError(
                f"requested ST-Link {requested_serial} was not found; available: {available}"
            )
        probe = matches[0]
    elif len(probes) == 1:
        probe = probes[0]
    elif not probes:
        raise ProgrammingError("no ST-Link detected; check its USB connection and target power")
    else:
        serials = ", ".join(p.serial_number for p in probes)
        raise ProgrammingError(f"multiple ST-Links detected; use --probe-serial: {serials}")
    progress(ProgressEvent("flash", f"Programming and verifying with {probe.serial_number}"))
    programmer.program_and_verify(probe, firmware_path)
    progress(ProgressEvent("flash", "Resetting target"))
    programmer.reset(probe)
    return probe
