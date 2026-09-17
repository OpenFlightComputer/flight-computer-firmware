"""Build-and-flash composition."""

from __future__ import annotations

from pathlib import Path

from openflightcomputer.firmware import ProgressCallback, build_firmware
from openflightcomputer.models import (
    FirmwareArtifact,
    FirmwareProfile,
    FlashOutcome,
    ProgressEvent,
)
from openflightcomputer.programmer import (
    ProgrammingError,
    Stm32CubeProgrammer,
    flash_firmware,
)


APPLICATION_METADATA_ADDRESS = 0x080DFF00
APPLICATION_FIRST_SECTOR = 4
APPLICATION_LAST_SECTOR = 10


def build_and_flash(
    profile: FirmwareProfile = "release",
    *,
    firmware_path: Path | None = None,
    probe_serial: str | None = None,
    programmer_path: Path | None = None,
    progress: ProgressCallback = lambda _event: None,
) -> FlashOutcome:
    artifact = (
        build_firmware(profile, progress=progress)
        if firmware_path is None
        else FirmwareArtifact(
            profile=profile,
            elf_path=firmware_path.expanduser().resolve(),
        )
    )
    programmer = Stm32CubeProgrammer.create(programmer_path)
    if artifact.bootloader_elf_path is None:
        probe = flash_firmware(
            programmer,
            artifact.elf_path,
            requested_serial=probe_serial,
            progress=progress,
        )
        return FlashOutcome(artifact=artifact, probe=probe)
    probes = programmer.discover_probes()
    if probe_serial is not None:
        probes = tuple(probe for probe in probes if probe.serial_number == probe_serial)
    if len(probes) != 1:
        raise ProgrammingError("exactly one matching ST-Link is required")
    probe = probes[0]
    if artifact.application_metadata_path is None:
        raise ProgrammingError(
            "combined SWD installation is missing application metadata"
        )
    progress(ProgressEvent("flash", "Programming resident bootloader"))
    programmer.program_and_verify(probe, artifact.bootloader_elf_path)
    progress(ProgressEvent("flash", "Erasing replaceable application sectors"))
    programmer.erase_sector_range(
        probe, APPLICATION_FIRST_SECTOR, APPLICATION_LAST_SECTOR
    )
    progress(ProgressEvent("flash", "Programming relocated flight application"))
    programmer.program_and_verify(probe, artifact.elf_path)
    progress(ProgressEvent("flash", "Committing verified application metadata"))
    programmer.program_binary_and_verify(
        probe, artifact.application_metadata_path, APPLICATION_METADATA_ADDRESS
    )
    programmer.reset(probe)
    return FlashOutcome(artifact=artifact, probe=probe)
