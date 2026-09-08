"""Validated receiver inspection data and tester-style terminal rendering."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from rich.console import Group
from rich.table import Table

from openflightcomputer.protocol import ProtocolError


CHANNEL_COUNT = 16
CHANNEL_MINIMUM = 172
CHANNEL_MAXIMUM = 1811


def _required_bool(data: dict[str, Any], key: str) -> bool:
    value = data.get(key)
    if type(value) is not bool:
        raise ProtocolError(f"receiver response {key} must be a boolean")
    return value


def _required_int(
    data: dict[str, Any], key: str, *, minimum: int, maximum: int
) -> int:
    value = data.get(key)
    if type(value) is not int or not minimum <= value <= maximum:
        raise ProtocolError(
            f"receiver response {key} must be an integer from {minimum} through {maximum}"
        )
    return value


def _required_float(
    data: dict[str, Any], key: str, *, minimum: float, maximum: float
) -> float:
    value = data.get(key)
    if type(value) not in {int, float} or not minimum <= value <= maximum:
        raise ProtocolError(
            f"receiver response {key} must be a number from {minimum} through {maximum}"
        )
    return float(value)


def _required_string(data: dict[str, Any], key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value:
        raise ProtocolError(f"receiver response {key} must be a string")
    return value


@dataclass(frozen=True)
class ReceiverSample:
    available: bool
    channels: tuple[int, ...] | None
    sequence: int | None
    age_us: int | None
    freshness: str
    roll: float | None
    pitch: float | None
    yaw: float | None
    throttle: float | None
    arm: bool | None
    failsafe_state: str
    failsafe_action: str
    stage_two_latched: bool
    recovery_ready: bool
    link_statistics_present: bool
    uplink_rssi_dbm: int
    uplink_link_quality_percent: int
    uplink_snr_db: int
    uart_bytes: int
    valid_frames: int
    crc_errors: int
    framing_errors: int
    dma_overruns: int
    dma_bytes_dropped: int

    @classmethod
    def from_response(cls, response: dict[str, Any]) -> "ReceiverSample":
        if response.get("type") != "response" or response.get("command") != "receiver":
            raise ProtocolError("expected a receiver response")
        available = _required_bool(response, "available")
        freshness = _required_string(response, "freshness")
        failsafe = response.get("failsafe")
        if not isinstance(failsafe, dict):
            raise ProtocolError("receiver response failsafe must be an object")

        channels: tuple[int, ...] | None = None
        sequence: int | None = None
        age_us: int | None = None
        roll: float | None = None
        pitch: float | None = None
        yaw: float | None = None
        throttle: float | None = None
        arm: bool | None = None
        if available:
            channel_values = response.get("channels")
            if not isinstance(channel_values, list) or len(channel_values) != CHANNEL_COUNT:
                raise ProtocolError("receiver response channels must contain 16 integers")
            if any(type(value) is not int or not 0 <= value <= 2047 for value in channel_values):
                raise ProtocolError("receiver response channels must contain uint11 values")
            channels = tuple(channel_values)
            sequence = _required_int(
                response, "sequence", minimum=0, maximum=0xFFFFFFFF
            )
            age_us = _required_int(
                response, "age_us", minimum=0, maximum=0xFFFFFFFFFFFFFFFF
            )
            normalized = response.get("normalized")
            if not isinstance(normalized, dict):
                raise ProtocolError("receiver response normalized must be an object")
            roll = _required_float(normalized, "roll", minimum=-1.0, maximum=1.0)
            pitch = _required_float(normalized, "pitch", minimum=-1.0, maximum=1.0)
            yaw = _required_float(normalized, "yaw", minimum=-1.0, maximum=1.0)
            throttle = _required_float(
                normalized, "throttle", minimum=0.0, maximum=1.0
            )
            arm = _required_bool(normalized, "arm")
        elif any(response.get(key) is not None for key in ("channels", "sequence", "age_us", "normalized")):
            raise ProtocolError("unavailable receiver data must use null snapshots")

        return cls(
            available=available,
            channels=channels,
            sequence=sequence,
            age_us=age_us,
            freshness=freshness,
            roll=roll,
            pitch=pitch,
            yaw=yaw,
            throttle=throttle,
            arm=arm,
            failsafe_state=_required_string(failsafe, "state"),
            failsafe_action=_required_string(failsafe, "action"),
            stage_two_latched=_required_bool(failsafe, "stage_two_latched"),
            recovery_ready=_required_bool(failsafe, "recovery_ready"),
            link_statistics_present=_required_bool(
                response, "link_statistics_present"
            ),
            uplink_rssi_dbm=_required_int(
                response, "uplink_rssi_dbm", minimum=-32768, maximum=32767
            ),
            uplink_link_quality_percent=_required_int(
                response,
                "uplink_link_quality_percent",
                minimum=0,
                maximum=255,
            ),
            uplink_snr_db=_required_int(
                response, "uplink_snr_db", minimum=-128, maximum=127
            ),
            uart_bytes=_required_int(
                response, "uart_bytes", minimum=0, maximum=0xFFFFFFFF
            ),
            valid_frames=_required_int(
                response, "valid_frames", minimum=0, maximum=0xFFFFFFFF
            ),
            crc_errors=_required_int(
                response, "crc_errors", minimum=0, maximum=0xFFFFFFFF
            ),
            framing_errors=_required_int(
                response, "framing_errors", minimum=0, maximum=0xFFFFFFFF
            ),
            dma_overruns=_required_int(
                response, "dma_overruns", minimum=0, maximum=0xFFFFFFFF
            ),
            dma_bytes_dropped=_required_int(
                response, "dma_bytes_dropped", minimum=0, maximum=0xFFFFFFFF
            ),
        )


@dataclass
class ReceiverView:
    sample: ReceiverSample | None = None
    channel_minima: list[int] | None = field(default=None)
    channel_maxima: list[int] | None = field(default=None)

    def update(self, response: dict[str, Any]) -> None:
        sample = ReceiverSample.from_response(response)
        self.sample = sample
        if sample.channels is None:
            return
        if self.channel_minima is None or self.channel_maxima is None:
            self.channel_minima = list(sample.channels)
            self.channel_maxima = list(sample.channels)
            return
        for index, value in enumerate(sample.channels):
            self.channel_minima[index] = min(self.channel_minima[index], value)
            self.channel_maxima[index] = max(self.channel_maxima[index], value)

    def render(self) -> Group:
        return Group(
            self._connection_table(),
            self._control_table(),
            self._channel_table(),
            "Move the transmitter controls; press Ctrl-C to stop." if self.sample else
            "Waiting for a receiver response.",
        )

    def _connection_table(self) -> Table:
        sample = self.sample
        table = Table(title="RadioMaster RP1 — CRSF receiver")
        table.add_column("Measurement")
        table.add_column("Value", justify="right")
        table.add_row(
            "Status",
            "Receiving control packets" if sample and sample.available else
            "Waiting for control packets",
        )
        table.add_row("Freshness", sample.freshness if sample else "—")
        table.add_row(
            "Failsafe state / action",
            f"{sample.failsafe_state} / {sample.failsafe_action}" if sample else "—",
        )
        table.add_row(
            "Stage 2 latched / recovery ready",
            f"{sample.stage_two_latched} / {sample.recovery_ready}" if sample else "—",
        )
        table.add_row(
            "Last control packet",
            "—" if not sample or sample.age_us is None else
            f"{sample.age_us / 1000.0:.1f} ms ago",
        )
        table.add_row(
            "Uplink link quality",
            self._link_value(sample.uplink_link_quality_percent if sample else None, "%"),
        )
        table.add_row(
            "Uplink RSSI",
            self._link_value(sample.uplink_rssi_dbm if sample else None, "dBm"),
        )
        table.add_row(
            "Uplink SNR",
            self._link_value(sample.uplink_snr_db if sample else None, "dB"),
        )
        table.add_row("UART bytes", str(sample.uart_bytes) if sample else "—")
        table.add_row("Valid CRSF frames", str(sample.valid_frames) if sample else "—")
        table.add_row(
            "CRC / framing errors",
            f"{sample.crc_errors} / {sample.framing_errors}" if sample else "—",
        )
        table.add_row(
            "DMA overruns / dropped bytes",
            f"{sample.dma_overruns} / {sample.dma_bytes_dropped}" if sample else "—",
        )
        return table

    def _control_table(self) -> Table:
        sample = self.sample
        table = Table(title="Normalized flight controls")
        table.add_column("Control")
        table.add_column("Channel")
        table.add_column("Raw", justify="right")
        table.add_column("Normalized", justify="right")
        controls = (
            ("Roll", 0, sample.roll if sample else None),
            ("Pitch", 1, sample.pitch if sample else None),
            ("Throttle", 2, sample.throttle if sample else None),
            ("Yaw", 3, sample.yaw if sample else None),
        )
        for name, channel, normalized in controls:
            raw = sample.channels[channel] if sample and sample.channels else None
            table.add_row(
                name,
                f"CH{channel + 1:02d}",
                "—" if raw is None else str(raw),
                "—" if normalized is None else f"{normalized:+.3f}",
            )
        arm_raw = sample.channels[4] if sample and sample.channels else None
        table.add_row(
            "Arm",
            "CH05",
            "—" if arm_raw is None else str(arm_raw),
            "—" if not sample or sample.arm is None else
            ("HIGH" if sample.arm else "LOW"),
        )
        return table

    def _channel_table(self) -> Table:
        sample = self.sample
        table = Table(title="Receiver channels")
        table.add_column("Channel")
        table.add_column("Raw", justify="right")
        table.add_column("Min", justify="right")
        table.add_column("Max", justify="right")
        table.add_column("Position")
        for index in range(CHANNEL_COUNT):
            value = sample.channels[index] if sample and sample.channels else None
            minimum = self.channel_minima[index] if self.channel_minima else None
            maximum = self.channel_maxima[index] if self.channel_maxima else None
            table.add_row(
                f"CH{index + 1:02d}",
                "—" if value is None else str(value),
                "—" if minimum is None else str(minimum),
                "—" if maximum is None else str(maximum),
                self._channel_bar(value),
            )
        return table

    def _link_value(self, value: int | None, unit: str) -> str:
        if not self.sample or not self.sample.link_statistics_present or value is None:
            return "—"
        return f"{value} {unit}"

    @staticmethod
    def _channel_bar(value: int | None) -> str:
        if value is None:
            return "—"
        width = 21
        bounded = min(CHANNEL_MAXIMUM, max(CHANNEL_MINIMUM, value))
        position = round(
            (bounded - CHANNEL_MINIMUM) * (width - 1) /
            (CHANNEL_MAXIMUM - CHANNEL_MINIMUM)
        )
        return "─" * position + "●" + "─" * (width - position - 1)
