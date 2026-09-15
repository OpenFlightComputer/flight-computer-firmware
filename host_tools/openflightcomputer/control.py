"""Validated control traces and a low-overhead live flight-control dashboard."""

from __future__ import annotations

import csv
import json
import math
from collections import deque
from datetime import datetime, timezone
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any
from uuid import uuid4

from rich.columns import Columns
from rich.console import Group
from rich.panel import Panel
from rich.table import Table

from openflightcomputer.protocol import ProtocolError


RECORD_FIELD_COUNT = 34
TRACE_SCHEMA_VERSION = 2
TRACE_FILE_VERSION = 1
AXES = ("Roll", "Pitch", "Yaw")
SYSTEM_STATES = ("BOOT", "INITIALIZING", "DISARMED", "ARMED", "FAILSAFE", "FAULT")
CONTROL_SOURCES = ("NONE", "USB_TEST", "RECEIVER")
FAILSAFE_STATES = (
    "UNAVAILABLE", "LIVE", "STALE_HOLD", "LOSS_HOLD", "STAGE_ONE",
    "STAGE_TWO_LATCHED", "STAGE_TWO_RECOVERING",
)
FAILSAFE_ACTIONS = ("NONE", "LIVE", "HOLD_LAST", "STAGE_ONE", "STOP")
IMU_FRESHNESS = ("UNAVAILABLE", "FRESH", "STALE", "LOST")
CONTROL_RESULTS = (
    "IDLE", "SUBMITTED", "FAILSAFE_ENTERED", "RECOVERED", "WAITING_FOR_IMU",
    "IMU_FAILSAFE_ENTERED", "CONTROL_FAILSAFE_ENTERED", "SUBMIT_ERROR",
    "FAILSAFE_ERROR", "RECOVERY_ERROR",
)
RATE_RESULTS = (
    "UPDATED", "SEEDED", "DISABLED", "NO_NEW_SAMPLE", "CONTINUITY_LOST",
    "INVALID_INPUT", "NOT_INITIALIZED",
)


def _integer(value: Any, name: str, *, minimum: int = 0) -> int:
    if type(value) is not int or value < minimum:
        raise ProtocolError(f"control trace {name} must be an integer >= {minimum}")
    return value


def _enum_name(value: Any, names: tuple[str, ...], name: str) -> str:
    index = _integer(value, name)
    if index >= len(names):
        raise ProtocolError(f"control trace {name} is invalid")
    return names[index]


def _scaled_vector(value: Any, count: int, scale: int, name: str) -> tuple[float, ...]:
    if not isinstance(value, list) or len(value) != count or any(type(item) is not int for item in value):
        raise ProtocolError(f"control trace {name} must contain {count} integers")
    return tuple(item / scale for item in value)


def _raw_vector(value: Any, count: int, name: str) -> tuple[int, ...]:
    if not isinstance(value, list) or len(value) != count or any(type(item) is not int for item in value):
        raise ProtocolError(f"control trace {name} must contain {count} integers")
    return tuple(value)


@dataclass(frozen=True)
class ControlTraceRecord:
    sequence: int
    timestamp_us: int
    imu_sequence: int
    event_flags: int
    state: str
    source: str
    failsafe_state: str
    failsafe_action: str
    imu_freshness: str
    control_result: str
    rate_result: str
    validity_flags: int
    dt_us: int
    receiver: tuple[float, ...]
    setpoint: tuple[float, ...]
    attitude: tuple[float, ...]
    desired_rates: tuple[float, ...]
    measured_rates: tuple[float, ...]
    pid: tuple[tuple[float, ...], ...]
    motors: tuple[float, ...]
    mixer_scale: float
    collective_shift: float
    mixer_saturated: bool
    raw_acceleration: tuple[int, ...]
    raw_gyroscope: tuple[int, ...]
    unfiltered_acceleration: tuple[float, ...]
    filtered_acceleration: tuple[float, ...]
    acceleration_magnitude: tuple[float, ...]
    unfiltered_accelerometer_attitude: tuple[float, ...]
    filtered_accelerometer_attitude: tuple[float, ...]
    corrected_gyroscope: tuple[float, ...]
    filtered_gyroscope: tuple[float, ...]
    gyro_predicted_attitude: tuple[float, ...]
    accelerometer_weight: float

    @classmethod
    def from_wire(cls, value: Any, scale: int) -> "ControlTraceRecord":
        if not isinstance(value, list) or len(value) != RECORD_FIELD_COUNT:
            raise ProtocolError("control trace record has an unsupported shape")
        pid_flat = _scaled_vector(value[18], 12, scale, "PID terms")
        if type(value[22]) is not int or value[22] not in (0, 1):
            raise ProtocolError("control trace saturation flag must be 0 or 1")
        return cls(
            sequence=_integer(value[0], "sequence"),
            timestamp_us=_integer(value[1], "timestamp"),
            imu_sequence=_integer(value[2], "IMU sequence"),
            event_flags=_integer(value[3], "event flags"),
            state=_enum_name(value[4], SYSTEM_STATES, "state"),
            source=_enum_name(value[5], CONTROL_SOURCES, "source"),
            failsafe_state=_enum_name(value[6], FAILSAFE_STATES, "failsafe state"),
            failsafe_action=_enum_name(value[7], FAILSAFE_ACTIONS, "failsafe action"),
            imu_freshness=_enum_name(value[8], IMU_FRESHNESS, "IMU freshness"),
            control_result=_enum_name(value[9], CONTROL_RESULTS, "control result"),
            rate_result=_enum_name(value[10], RATE_RESULTS, "rate result"),
            validity_flags=_integer(value[11], "validity flags"),
            dt_us=_integer(value[12], "dt"),
            receiver=_scaled_vector(value[13], 4, scale, "receiver values"),
            setpoint=_scaled_vector(value[14], 4, scale, "setpoint"),
            attitude=_scaled_vector(value[15], 2, scale, "attitude"),
            desired_rates=_scaled_vector(value[16], 3, scale, "desired rates"),
            measured_rates=_scaled_vector(value[17], 3, scale, "measured rates"),
            pid=tuple(tuple(pid_flat[index:index + 4]) for index in range(0, 12, 4)),
            motors=_scaled_vector(value[19], 4, scale, "motor values"),
            mixer_scale=_integer(value[20], "mixer scale", minimum=-0x80000000) / scale,
            collective_shift=_integer(value[21], "collective shift", minimum=-0x80000000) / scale,
            mixer_saturated=bool(value[22]),
            raw_acceleration=_raw_vector(value[23], 3, "raw acceleration"),
            raw_gyroscope=_raw_vector(value[24], 3, "raw gyroscope"),
            unfiltered_acceleration=_scaled_vector(value[25], 3, scale, "unfiltered acceleration"),
            filtered_acceleration=_scaled_vector(value[26], 3, scale, "filtered acceleration"),
            acceleration_magnitude=_scaled_vector(value[27], 2, scale, "acceleration magnitude"),
            unfiltered_accelerometer_attitude=_scaled_vector(value[28], 2, scale, "unfiltered accelerometer attitude"),
            filtered_accelerometer_attitude=_scaled_vector(value[29], 2, scale, "filtered accelerometer attitude"),
            corrected_gyroscope=_scaled_vector(value[30], 3, scale, "corrected gyroscope"),
            filtered_gyroscope=_scaled_vector(value[31], 3, scale, "filtered gyroscope"),
            gyro_predicted_attitude=_scaled_vector(value[32], 2, scale, "gyro-predicted attitude"),
            accelerometer_weight=_integer(value[33], "accelerometer weight", minimum=-0x80000000) / scale,
        )


@dataclass(frozen=True)
class ControlTraceBatch:
    capture_id: int
    level: str
    capturing: bool
    pending_records: int
    dropped_records: int
    records: tuple[ControlTraceRecord, ...]

    @classmethod
    def from_response(cls, response: dict[str, Any]) -> "ControlTraceBatch":
        if response.get("type") != "response" or response.get("command") != "control_trace_read":
            raise ProtocolError("expected a control_trace_read response")
        if response.get("schema_version") != TRACE_SCHEMA_VERSION:
            raise ProtocolError("unsupported control trace schema")
        level = response.get("level")
        if level not in {"OFF", "EVENTS", "LOW_RATE", "HIGH_RATE", "FULL_RATE"}:
            raise ProtocolError("control trace level is invalid")
        capturing = response.get("capturing")
        if type(capturing) is not bool:
            raise ProtocolError("control trace capturing must be a boolean")
        scale = _integer(response.get("scale"), "scale", minimum=1)
        wire_records = response.get("records")
        if not isinstance(wire_records, list):
            raise ProtocolError("control trace records must be an array")
        return cls(
            capture_id=_integer(response.get("capture_id"), "capture ID"),
            level=level,
            capturing=capturing,
            pending_records=_integer(response.get("pending_records"), "pending records"),
            dropped_records=_integer(response.get("dropped_records"), "dropped records"),
            records=tuple(ControlTraceRecord.from_wire(record, scale) for record in wire_records),
        )


@dataclass(frozen=True)
class ControlTraceMetadata:
    trace_id: str
    created_at: str
    firmware_version: str
    build_id: str
    configuration_source: str
    configuration_schema_version: int | None
    configuration: dict[str, Any]
    capture_id: int
    capture_level: str
    dropped_records: int

    @classmethod
    def create(
        cls,
        status: dict[str, Any],
        configuration_response: dict[str, Any],
        view: "ControlTraceView",
    ) -> "ControlTraceMetadata":
        configuration = configuration_response.get("configuration")
        if not isinstance(configuration, dict):
            configuration = {}
        schema_version = configuration.get("schema_version")
        if type(schema_version) is not int:
            schema_version = None
        return cls(
            trace_id=uuid4().hex[:8],
            created_at=datetime.now(timezone.utc).isoformat(),
            firmware_version=str(status.get("firmware_version", "unknown")),
            build_id=str(status.get("build_id", "unknown")),
            configuration_source=str(configuration_response.get("source", "unknown")),
            configuration_schema_version=schema_version,
            configuration=configuration,
            capture_id=view.capture_id,
            capture_level=view.level,
            dropped_records=view.dropped_records,
        )


def _sparkline(values: deque[float]) -> str:
    blocks = "▁▂▃▄▅▆▇█"
    if not values:
        return "—"
    low, high = min(values), max(values)
    if math.isclose(low, high):
        return blocks[3] * len(values)
    return "".join(blocks[min(7, int((value - low) * 7 / (high - low)))] for value in values)


@dataclass
class ControlTraceView:
    latest: ControlTraceRecord | None = None
    capture_id: int = 0
    level: str = "OFF"
    capturing: bool = False
    pending_records: int = 0
    dropped_records: int = 0
    records: list[ControlTraceRecord] = field(default_factory=list)
    rate_history: list[deque[float]] = field(
        default_factory=lambda: [deque(maxlen=32) for _ in range(6)]
    )

    def update(self, response: dict[str, Any]) -> ControlTraceBatch:
        batch = ControlTraceBatch.from_response(response)
        self.capture_id = batch.capture_id
        self.level = batch.level
        self.capturing = batch.capturing
        self.pending_records = batch.pending_records
        self.dropped_records = batch.dropped_records
        for record in batch.records:
            if self.latest is not None and record.sequence <= self.latest.sequence:
                continue
            self.latest = record
            self.records.append(record)
            for axis in range(3):
                self.rate_history[axis * 2].append(record.desired_rates[axis])
                self.rate_history[axis * 2 + 1].append(record.measured_rates[axis])
        return batch

    def render(self) -> Group:
        return Group(
            self._status_table(),
            Columns([self._attitude_panel(), self._motor_panel()], equal=True),
            self._imu_pipeline_table(),
            self._rate_table(),
            "Move the flight computer and transmitter; press Ctrl-C to stop.",
        )

    def _status_table(self) -> Table:
        record = self.latest
        table = Table(title="Flight-control trace")
        for heading in ("Capture", "State / source", "Failsafe", "IMU", "Control", "Buffer"):
            table.add_column(heading)
        table.add_row(
            f"{self.capture_id} · {self.level}",
            f"{record.state} / {record.source}" if record else "—",
            f"{record.failsafe_state} / {record.failsafe_action}" if record else "—",
            f"{record.imu_freshness} · seq {record.imu_sequence}" if record else "—",
            f"{record.control_result} / {record.rate_result}" if record else "—",
            f"pending {self.pending_records} · dropped {self.dropped_records}",
        )
        return table

    def _attitude_panel(self) -> Panel:
        if self.latest is None:
            return Panel("Waiting for samples", title="Attitude")
        roll, pitch = self.latest.attitude
        width, height = 31, 9
        grid = [[" " for _ in range(width)] for _ in range(height)]
        slope = math.tan(math.radians(max(-65.0, min(65.0, roll)))) * 0.35
        shift = max(-3.0, min(3.0, pitch / 10.0))
        for x in range(width):
            y = round((height - 1) / 2 + shift + slope * (x - (width - 1) / 2))
            if 0 <= y < height:
                grid[y][x] = "─"
        grid[height // 2][width // 2] = "✚"
        picture = "\n".join("".join(row) for row in grid)
        return Panel(f"{picture}\nroll {roll:+6.1f}°   pitch {pitch:+6.1f}°", title="Attitude")

    def _motor_panel(self) -> Panel:
        if self.latest is None:
            return Panel("Waiting for samples", title="Quad-X motor output")
        m1, m2, m3, m4 = self.latest.motors
        body = (
            f"M1 front-left {m1:6.1%}       {m3:6.1%} front-right M3\n"
            "             ╲                 ╱\n"
            "                       ▲\n"
            "             ╱                 ╲\n"
            f"M2 rear-left  {m2:6.1%}       {m4:6.1%} rear-right M4\n\n"
            f"scale {self.latest.mixer_scale:.3f} · shift "
            f"{self.latest.collective_shift:+.3f} · saturated "
            f"{'YES' if self.latest.mixer_saturated else 'no'}"
        )
        return Panel(body, title="Quad-X motor output")

    def _rate_table(self) -> Table:
        table = Table(title="Rate controller")
        for heading in ("Axis", "Desired", "Measured", "P", "I", "D", "Output", "History desired / measured"):
            table.add_column(heading, justify="right" if heading != "Axis" else "left")
        if self.latest is None:
            return table
        for axis, name in enumerate(AXES):
            p, i, d, total = self.latest.pid[axis]
            table.add_row(
                name,
                f"{self.latest.desired_rates[axis]:+.1f}",
                f"{self.latest.measured_rates[axis]:+.1f}",
                f"{p:+.3f}", f"{i:+.3f}", f"{d:+.3f}", f"{total:+.3f}",
                f"{_sparkline(self.rate_history[axis * 2])} / "
                f"{_sparkline(self.rate_history[axis * 2 + 1])}",
            )
        return table

    def _imu_pipeline_table(self) -> Table:
        table = Table(title="IMU processing pipeline")
        for heading in ("Signal", "X / roll", "Y / pitch", "Z", "Magnitude / weight"):
            table.add_column(heading, justify="right" if heading != "Signal" else "left")
        if self.latest is None:
            return table
        record = self.latest
        table.add_row(
            "Raw counts",
            str(record.raw_acceleration[0]), str(record.raw_acceleration[1]),
            str(record.raw_acceleration[2]), "accelerometer",
        )
        table.add_row(
            "Accel unfiltered (g)",
            f"{record.unfiltered_acceleration[0]:+.3f}",
            f"{record.unfiltered_acceleration[1]:+.3f}",
            f"{record.unfiltered_acceleration[2]:+.3f}",
            f"{record.acceleration_magnitude[0]:.3f} g",
        )
        table.add_row(
            "Accel filtered (g)",
            f"{record.filtered_acceleration[0]:+.3f}",
            f"{record.filtered_acceleration[1]:+.3f}",
            f"{record.filtered_acceleration[2]:+.3f}",
            f"{record.acceleration_magnitude[1]:.3f} g",
        )
        table.add_row(
            "Accel attitude (raw / filtered)",
            f"{record.unfiltered_accelerometer_attitude[0]:+.1f}° / "
            f"{record.filtered_accelerometer_attitude[0]:+.1f}°",
            f"{record.unfiltered_accelerometer_attitude[1]:+.1f}° / "
            f"{record.filtered_accelerometer_attitude[1]:+.1f}°",
            "—", f"weight {record.accelerometer_weight:.3f}",
        )
        table.add_row(
            "Gyro corrected / filtered (dps)",
            f"{record.corrected_gyroscope[0]:+.1f} / {record.filtered_gyroscope[0]:+.1f}",
            f"{record.corrected_gyroscope[1]:+.1f} / {record.filtered_gyroscope[1]:+.1f}",
            f"{record.corrected_gyroscope[2]:+.1f} / {record.filtered_gyroscope[2]:+.1f}",
            "—",
        )
        table.add_row(
            "Gyro-predicted attitude",
            f"{record.gyro_predicted_attitude[0]:+.1f}°",
            f"{record.gyro_predicted_attitude[1]:+.1f}°",
            "—", "before accel correction",
        )
        return table


def _versioned_trace_path(path: Path, trace_id: str) -> Path:
    suffix = path.suffix or ".json"
    stem = path.stem if path.suffix else path.name
    return path.with_name(
        f"{stem}-v{TRACE_FILE_VERSION}-{trace_id}{suffix}"
    )


def export_trace(
    path: Path,
    records: list[ControlTraceRecord],
    metadata: ControlTraceMetadata,
) -> Path:
    path = _versioned_trace_path(path, metadata.trace_id)
    if path.exists():
        raise FileExistsError(f"refusing to overwrite existing trace: {path}")
    rows = [asdict(record) for record in records]
    if path.suffix.lower() == ".csv":
        if not rows:
            path.write_text("", encoding="utf-8")
            return path
        common = {
            "trace_file_version": TRACE_FILE_VERSION,
            "trace_schema_version": TRACE_SCHEMA_VERSION,
            **asdict(metadata),
        }
        with path.open("w", encoding="utf-8", newline="") as output:
            fieldnames = [*common.keys(), *rows[0].keys()]
            writer = csv.DictWriter(output, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows({
                key: json.dumps(value) if isinstance(value, (dict, tuple)) else value
                for key, value in {**common, **row}.items()
            } for row in rows)
        return path
    document = {
        "trace_file_version": TRACE_FILE_VERSION,
        "trace_schema_version": TRACE_SCHEMA_VERSION,
        "metadata": asdict(metadata),
        "records": rows,
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    return path
