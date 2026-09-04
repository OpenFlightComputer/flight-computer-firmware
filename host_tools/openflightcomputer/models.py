"""Transport-neutral values shared by CLI and future frontends."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Literal


FirmwareProfile = Literal["debug", "release"]


@dataclass(frozen=True, slots=True)
class ProgressEvent:
    """One presentation-neutral workflow update."""

    operation: str
    message: str


@dataclass(frozen=True, slots=True)
class FirmwareArtifact:
    """One built or user-supplied firmware image."""

    profile: FirmwareProfile
    elf_path: Path
    firmware_version: str | None = None
    build_id: str | None = None


@dataclass(frozen=True, slots=True)
class Probe:
    """One uniquely selectable debug probe."""

    serial_number: str


@dataclass(frozen=True, slots=True)
class FlashOutcome:
    artifact: FirmwareArtifact
    probe: Probe


@dataclass(frozen=True, slots=True)
class SerialPort:
    device: str
    vid: int | None
    pid: int | None
    description: str | None = None
    serial_number: str | None = None


@dataclass(frozen=True, slots=True)
class SmokeCheck:
    name: str
    passed: bool
    detail: str


@dataclass(frozen=True, slots=True)
class SmokeResult:
    passed: bool
    started_at: str
    completed_at: str
    port: str
    status: dict[str, Any]
    health: dict[str, Any]
    checks: tuple[SmokeCheck, ...]
    artifact: FirmwareArtifact | None = None
    probe: Probe | None = None
    observed_logs: tuple[dict[str, Any], ...] = field(default_factory=tuple)
