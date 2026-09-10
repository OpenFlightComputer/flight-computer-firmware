"""Validated IMU inspection data and a tester-style terminal view."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from rich.console import Group
from rich.table import Table

from openflightcomputer.protocol import ProtocolError


ACCELERATION_COUNTS_PER_G = 16384.0
GYROSCOPE_COUNTS_PER_DEGREE_PER_SECOND = 16.384
GYROSCOPE_DISPLAY_LIMIT_DPS = 250.0


def _required_bool(data: dict[str, Any], key: str) -> bool:
    value = data.get(key)
    if type(value) is not bool:
        raise ProtocolError(f"IMU response {key} must be a boolean")
    return value


def _required_int(
    data: dict[str, Any], key: str, *, minimum: int, maximum: int
) -> int:
    value = data.get(key)
    if type(value) is not int or not minimum <= value <= maximum:
        raise ProtocolError(
            f"IMU response {key} must be an integer from {minimum} through {maximum}"
        )
    return value


def _required_object(data: dict[str, Any], key: str) -> dict[str, Any]:
    value = data.get(key)
    if not isinstance(value, dict):
        raise ProtocolError(f"IMU response {key} must be an object")
    return value


def _axis_triplet(data: dict[str, Any], key: str) -> tuple[int, int, int]:
    axes = _required_object(data, key)
    return tuple(
        _required_int(axes, axis, minimum=-0x80000000, maximum=0x7FFFFFFF)
        for axis in ("x", "y", "z")
    )


@dataclass(frozen=True)
class ImuSample:
    available: bool
    sequence: int | None
    age_us: int | None
    freshness: str
    acceleration_raw: tuple[int, int, int] | None
    gyroscope_raw: tuple[int, int, int] | None
    gyroscope_corrected_raw: tuple[int, int, int] | None
    calibration_state: str
    calibration_progress_permille: int
    calibration_samples: int
    calibration_restarts: int
    calibration_bias_raw: tuple[int, int, int] | None
    reads: int
    published: int
    source_errors: int
    task_executions: int | None
    task_last_execution_us: int | None
    task_maximum_execution_us: int | None
    task_overruns: int | None
    task_missed_releases: int | None
    high_rate_budget_us: int
    high_rate_utilization_permille: int

    @classmethod
    def from_response(cls, response: dict[str, Any]) -> "ImuSample":
        if response.get("type") != "response" or response.get("command") != "imu":
            raise ProtocolError("expected an IMU response")
        available = _required_bool(response, "available")
        freshness = response.get("freshness")
        if freshness not in {"UNAVAILABLE", "FRESH", "STALE", "LOST"}:
            raise ProtocolError("IMU response freshness is invalid")
        service = _required_object(response, "service")
        high_rate = _required_object(response, "high_rate")
        calibration = _required_object(response, "calibration")
        calibration_state = calibration.get("state")
        if calibration_state not in {"SETTLING", "COLLECTING", "READY"}:
            raise ProtocolError("IMU calibration state is invalid")

        sequence = age_us = None
        acceleration = gyroscope = corrected_gyroscope = calibration_bias = None
        task_values: tuple[int | None, ...] = (None,) * 5
        if available:
            sequence = _required_int(
                response, "sequence", minimum=0, maximum=0xFFFFFFFFFFFFFFFF
            )
            age_us = _required_int(
                response, "age_us", minimum=0, maximum=0xFFFFFFFFFFFFFFFF
            )
            acceleration = _axis_triplet(response, "acceleration_raw")
            gyroscope = _axis_triplet(response, "gyroscope_raw")
            if response.get("gyroscope_corrected_raw") is not None:
                corrected_gyroscope = _axis_triplet(
                    response, "gyroscope_corrected_raw"
                )
            task = _required_object(response, "task")
            task_values = tuple(
                _required_int(task, key, minimum=0, maximum=0xFFFFFFFF)
                for key in (
                    "executions",
                    "last_execution_us",
                    "maximum_execution_us",
                    "overruns",
                    "missed_releases",
                )
            )
        elif any(
            response.get(key) is not None
            for key in (
                "sequence", "age_us", "acceleration_raw", "gyroscope_raw",
                "gyroscope_corrected_raw", "task",
            )
        ):
            raise ProtocolError("unavailable IMU data must use null snapshots")
        if calibration.get("bias_raw") is not None:
            calibration_bias = _axis_triplet(calibration, "bias_raw")
        if calibration_state == "READY":
            if calibration_bias is None or (available and corrected_gyroscope is None):
                raise ProtocolError("ready IMU calibration must include bias and corrected gyro")
        elif calibration_bias is not None or corrected_gyroscope is not None:
            raise ProtocolError("unfinished IMU calibration must use null corrected values")

        return cls(
            available=available,
            sequence=sequence,
            age_us=age_us,
            freshness=freshness,
            acceleration_raw=acceleration,
            gyroscope_raw=gyroscope,
            gyroscope_corrected_raw=corrected_gyroscope,
            calibration_state=calibration_state,
            calibration_progress_permille=_required_int(
                calibration, "progress_permille", minimum=0, maximum=1000
            ),
            calibration_samples=_required_int(
                calibration, "samples", minimum=0, maximum=0xFFFFFFFF
            ),
            calibration_restarts=_required_int(
                calibration, "restarts", minimum=0, maximum=0xFFFFFFFF
            ),
            calibration_bias_raw=calibration_bias,
            reads=_required_int(service, "reads", minimum=0, maximum=0xFFFFFFFF),
            published=_required_int(
                service, "published", minimum=0, maximum=0xFFFFFFFF
            ),
            source_errors=_required_int(
                service, "source_errors", minimum=0, maximum=0xFFFFFFFF
            ),
            task_executions=task_values[0],
            task_last_execution_us=task_values[1],
            task_maximum_execution_us=task_values[2],
            task_overruns=task_values[3],
            task_missed_releases=task_values[4],
            high_rate_budget_us=_required_int(
                high_rate, "budget_us", minimum=0, maximum=0xFFFFFFFF
            ),
            high_rate_utilization_permille=_required_int(
                high_rate, "utilization_permille", minimum=0, maximum=0xFFFFFFFF
            ),
        )

    @property
    def acceleration_g(self) -> tuple[float, float, float] | None:
        if self.acceleration_raw is None:
            return None
        return tuple(value / ACCELERATION_COUNTS_PER_G for value in self.acceleration_raw)

    @property
    def gyroscope_dps(self) -> tuple[float, float, float] | None:
        if self.gyroscope_raw is None:
            return None
        return tuple(
            value / GYROSCOPE_COUNTS_PER_DEGREE_PER_SECOND
            for value in self.gyroscope_raw
        )

    @property
    def corrected_gyroscope_dps(self) -> tuple[float, float, float] | None:
        if self.gyroscope_corrected_raw is None:
            return None
        return tuple(
            value / GYROSCOPE_COUNTS_PER_DEGREE_PER_SECOND
            for value in self.gyroscope_corrected_raw
        )

    @property
    def calibration_bias_dps(self) -> tuple[float, float, float] | None:
        if self.calibration_bias_raw is None:
            return None
        return tuple(
            value / GYROSCOPE_COUNTS_PER_DEGREE_PER_SECOND
            for value in self.calibration_bias_raw
        )


def _bar(value: float, limit: float, width: int = 21) -> str:
    clamped = max(-limit, min(limit, value))
    center = width // 2
    position = round(center + (clamped / limit) * center)
    cells = ["·"] * width
    cells[center] = "│"
    cells[position] = "●"
    return "".join(cells)


@dataclass
class ImuView:
    sample: ImuSample | None = None

    def update(self, response: dict[str, Any]) -> None:
        self.sample = ImuSample.from_response(response)

    def render(self) -> Group:
        return Group(
            self._status_table(),
            self._measurement_table(),
            "Move the flight computer; press Ctrl-C to stop."
            if self.sample
            else "Waiting for an IMU response.",
        )

    def _status_table(self) -> Table:
        sample = self.sample
        table = Table(title="BMI270 — body-axis IMU")
        table.add_column("Measurement")
        table.add_column("Value", justify="right")
        table.add_row("Status", "Available" if sample and sample.available else "Unavailable")
        table.add_row("Freshness", sample.freshness if sample else "—")
        table.add_row(
            "Sample age",
            f"{sample.age_us / 1000.0:.3f} ms"
            if sample and sample.age_us is not None
            else "—",
        )
        table.add_row("Sequence", str(sample.sequence) if sample else "—")
        table.add_row(
            "Gyro calibration",
            (f"{sample.calibration_state} "
             f"({sample.calibration_progress_permille / 10:.1f}%, "
             f"{sample.calibration_samples} samples, "
             f"{sample.calibration_restarts} restarts)")
            if sample else "—",
        )
        table.add_row(
            "Gyro bias X / Y / Z",
            " / ".join(f"{value:+.3f} °/s" for value in sample.calibration_bias_dps)
            if sample and sample.calibration_bias_dps else "—",
        )
        table.add_row(
            "Reads / published / errors",
            f"{sample.reads} / {sample.published} / {sample.source_errors}"
            if sample
            else "—",
        )
        table.add_row(
            "IMU task last / maximum",
            f"{sample.task_last_execution_us} / {sample.task_maximum_execution_us} µs"
            if sample and sample.task_last_execution_us is not None
            else "—",
        )
        table.add_row(
            "IMU overruns / missed releases",
            f"{sample.task_overruns} / {sample.task_missed_releases}"
            if sample and sample.task_overruns is not None
            else "—",
        )
        table.add_row(
            "1 kHz worst-case budget",
            f"{sample.high_rate_budget_us} µs ({sample.high_rate_utilization_permille / 10:.1f}%)"
            if sample
            else "—",
        )
        return table

    def _measurement_table(self) -> Table:
        sample = self.sample
        acceleration = sample.acceleration_g if sample else None
        gyroscope = (
            sample.corrected_gyroscope_dps or sample.gyroscope_dps
            if sample else None
        )
        table = Table(title="X forward · Y right · Z down")
        table.add_column("Axis")
        table.add_column("Acceleration", justify="right")
        table.add_column("±2 g")
        table.add_column("Rotation", justify="right")
        table.add_column("±250 °/s")
        for index, axis in enumerate(("X", "Y", "Z")):
            table.add_row(
                axis,
                f"{acceleration[index]:+.3f} g" if acceleration else "—",
                _bar(acceleration[index], 2.0) if acceleration else "—",
                f"{gyroscope[index]:+.2f} °/s" if gyroscope else "—",
                _bar(gyroscope[index], GYROSCOPE_DISPLAY_LIMIT_DPS)
                if gyroscope
                else "—",
            )
        return table
