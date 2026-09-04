"""Non-arming flight-firmware smoke workflow."""

from __future__ import annotations

from collections.abc import Callable
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from openflightcomputer.device import UsbCdcConnection, wait_for_flight_port
from openflightcomputer.firmware import ProgressCallback
from openflightcomputer.models import (
    FirmwareArtifact,
    FirmwareProfile,
    FlashOutcome,
    ProgressEvent,
    SmokeCheck,
    SmokeResult,
)
from openflightcomputer.protocol import JsonProtocolClient
from openflightcomputer.workflows.flash import build_and_flash


FlashWorkflow = Callable[..., FlashOutcome]


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _check(name: str, passed: bool, detail: str) -> SmokeCheck:
    return SmokeCheck(name=name, passed=passed, detail=detail)


def run_smoke(
    profile: FirmwareProfile = "release",
    *,
    flash: bool = True,
    firmware_path: Path | None = None,
    probe_serial: str | None = None,
    programmer_path: Path | None = None,
    requested_port: str | Path | None = None,
    timeout_seconds: float = 10.0,
    progress: ProgressCallback = lambda _event: None,
    flash_workflow: FlashWorkflow = build_and_flash,
    port_waiter=wait_for_flight_port,
    connection_factory=UsbCdcConnection.open,
) -> SmokeResult:
    started_at = _utc_now()
    flash_outcome: FlashOutcome | None = None
    artifact: FirmwareArtifact | None = None
    if flash:
        flash_outcome = flash_workflow(
            profile,
            firmware_path=firmware_path,
            probe_serial=probe_serial,
            programmer_path=programmer_path,
            progress=progress,
        )
        artifact = flash_outcome.artifact
    elif firmware_path is not None:
        raise ValueError("--firmware cannot be used together with --no-flash")

    progress(ProgressEvent("device", "Waiting for flight-firmware USB CDC"))
    port = port_waiter(requested_port, timeout_seconds=timeout_seconds)
    logs: list[dict[str, Any]] = []

    def observe(message: dict[str, Any]) -> None:
        if message.get("type") == "log":
            logs.append(message)

    with connection_factory(port) as connection:
        client = JsonProtocolClient(connection)
        progress(ProgressEvent("smoke", "Requesting status"))
        status = client.request("status", timeout_seconds=timeout_seconds, observer=observe)
        progress(ProgressEvent("smoke", "Requesting health"))
        health = client.request("health", timeout_seconds=timeout_seconds, observer=observe)

    checks = (
        _check("status_ok", status.get("ok") is True, "status response accepted"),
        _check(
            "disarmed",
            status.get("state") == "DISARMED",
            f"state={status.get('state')}",
        ),
        _check(
            "identity",
            isinstance(status.get("firmware_version"), str)
            and bool(status.get("firmware_version"))
            and isinstance(status.get("build_id"), str)
            and bool(status.get("build_id")),
            f"version={status.get('firmware_version')} build={status.get('build_id')}",
        ),
        _check(
            "flashed_identity",
            artifact is None
            or artifact.build_id is None
            or (
                status.get("firmware_version") == artifact.firmware_version
                and status.get("build_id") == artifact.build_id
            ),
            "running identity matches flashed artifact"
            if artifact is not None and artifact.build_id is not None
            else "not checked for an untracked or pre-existing artifact",
        ),
        _check("health_ok", health.get("health") == "OK", f"health={health.get('health')}"),
        _check(
            "fault_data_complete",
            health.get("fault_data_complete") is True,
            f"fault_data_complete={health.get('fault_data_complete')}",
        ),
        _check(
            "no_dropped_faults",
            health.get("dropped_fault_count") == 0,
            f"dropped_fault_count={health.get('dropped_fault_count')}",
        ),
    )
    return SmokeResult(
        passed=all(check.passed for check in checks),
        started_at=started_at,
        completed_at=_utc_now(),
        port=port.device,
        status=status,
        health=health,
        checks=checks,
        artifact=artifact,
        probe=flash_outcome.probe if flash_outcome is not None else None,
        observed_logs=tuple(logs),
    )
