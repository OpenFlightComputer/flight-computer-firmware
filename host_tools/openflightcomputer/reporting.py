"""Stable JSON conversion and report persistence."""

from __future__ import annotations

import json
from dataclasses import asdict
from pathlib import Path
from typing import Any

from openflightcomputer.models import SmokeResult


def smoke_report_data(result: SmokeResult) -> dict[str, Any]:
    data = asdict(result)
    if result.artifact is not None:
        data["artifact"]["elf_path"] = str(result.artifact.elf_path)
    return data


def write_smoke_report(result: SmokeResult, destination: Path) -> Path:
    path = destination.expanduser().resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(smoke_report_data(result), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)
    return path
