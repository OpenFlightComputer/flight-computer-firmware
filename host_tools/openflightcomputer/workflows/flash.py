"""Build-and-flash composition."""

from __future__ import annotations

from pathlib import Path

from openflightcomputer.firmware import ProgressCallback, build_firmware
from openflightcomputer.models import FirmwareArtifact, FirmwareProfile, FlashOutcome
from openflightcomputer.programmer import Stm32CubeProgrammer, flash_firmware


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
        else FirmwareArtifact(profile=profile, elf_path=firmware_path.expanduser().resolve())
    )
    programmer = Stm32CubeProgrammer.create(programmer_path)
    probe = flash_firmware(
        programmer,
        artifact.elf_path,
        requested_serial=probe_serial,
        progress=progress,
    )
    return FlashOutcome(artifact=artifact, probe=probe)
