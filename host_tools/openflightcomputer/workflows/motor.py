"""Constrained, lease-based motor bench-test workflow."""

from __future__ import annotations

import math
import time
from collections.abc import Callable
from typing import Any, Protocol

from openflightcomputer.device import UsbCdcConnection, wait_for_flight_port
from openflightcomputer.protocol import JsonProtocolClient


MINIMUM_ACTIVE_THROTTLE = 0.001
MAXIMUM_THROTTLE = 1.0
HEARTBEAT_SECONDS = 0.02
ESC_PREPARE_SECONDS = 5.0
CLEANUP_ZERO_WRITES = 5
MotorProgress = Callable[[str], None]


class CommandClient(Protocol):
    def request(
        self,
        command: str,
        *,
        parameters: dict[str, object] | None = None,
        timeout_seconds: float = 2.0,
    ) -> dict[str, Any]: ...


def validate_motor_test(motor: int, throttle: float, duration_seconds: float) -> None:
    if motor not in range(1, 5):
        raise ValueError("motor must be between 1 and 4")
    if (
        not math.isfinite(throttle)
        or not MINIMUM_ACTIVE_THROTTLE < round(throttle, 6) <= MAXIMUM_THROTTLE
    ):
        raise ValueError("throttle must be greater than 0.001 and no more than 1.0")
    if not math.isfinite(duration_seconds) or duration_seconds <= 0.0:
        raise ValueError("duration must be a positive finite number")


def _send_motor(client: CommandClient, motor: int, throttle: float) -> None:
    client.request(
        "motor_test",
        parameters={"motor": motor, "throttle": round(throttle, 6)},
    )


def _report(progress: MotorProgress | None, message: str) -> None:
    if progress is not None:
        progress(message)


def _send_for(
    client: CommandClient,
    motor: int,
    throttle: float,
    duration_seconds: float,
    *,
    monotonic: Callable[[], float],
    sleeper: Callable[[float], None],
) -> int:
    deadline = monotonic() + duration_seconds
    count = 0
    while True:
        _send_motor(client, motor, throttle)
        count += 1
        remaining = deadline - monotonic()
        if remaining <= 0:
            return count
        sleeper(min(HEARTBEAT_SECONDS, remaining))


def _require_armed(client: CommandClient) -> None:
    status = client.request("status")
    if status.get("state") != "ARMED":
        raise ValueError("flight computer must already be ARMED; run `./ofc device arm`")


def _cleanup(
    client: CommandClient,
    motor: int,
    *,
    sleeper: Callable[[float], None],
) -> BaseException | None:
    cleanup_error: BaseException | None = None
    for _ in range(CLEANUP_ZERO_WRITES):
        try:
            _send_motor(client, motor, 0.0)
            sleeper(HEARTBEAT_SECONDS)
        except (Exception, KeyboardInterrupt) as error:
            cleanup_error = cleanup_error or error
    try:
        client.request("disarm")
    except (Exception, KeyboardInterrupt) as error:
        cleanup_error = cleanup_error or error
    return cleanup_error


def execute_motor_test(
    client: CommandClient,
    motor: int,
    throttle: float,
    duration_seconds: float,
    *,
    monotonic: Callable[[], float] = time.monotonic,
    sleeper: Callable[[float], None] = time.sleep,
    progress: MotorProgress | None = None,
) -> int:
    """Run one test and always attempt zero-output plus disarm cleanup."""
    validate_motor_test(motor, throttle, duration_seconds)
    _require_armed(client)

    sent = 0
    active_error: BaseException | None = None
    try:
        _report(progress, "PREPARING: zero throttle")
        _send_for(
            client,
            motor,
            0.0,
            ESC_PREPARE_SECONDS,
            monotonic=monotonic,
            sleeper=sleeper,
        )
        _report(progress, f"ACTIVE: motor {motor} at {throttle:.1%}")
        sent = _send_for(
            client,
            motor,
            throttle,
            duration_seconds,
            monotonic=monotonic,
            sleeper=sleeper,
        )
    except BaseException as error:
        active_error = error
        raise
    finally:
        _report(progress, "CLEANUP: zero throttle")
        cleanup_error = _cleanup(client, motor, sleeper=sleeper)
        if cleanup_error is None:
            _report(progress, "DISARMED")
        if active_error is None and cleanup_error is not None:
            raise cleanup_error
    return sent


def run_motor_test(
    motor: int,
    throttle: float,
    duration_seconds: float,
    *,
    requested_port: str | None = None,
    timeout_seconds: float = 10.0,
    progress: MotorProgress | None = None,
) -> int:
    """Find the board, retain one USB session, and run the safe test workflow."""
    validate_motor_test(motor, throttle, duration_seconds)
    port = wait_for_flight_port(requested_port, timeout_seconds=timeout_seconds)
    with UsbCdcConnection.open(port) as connection:
        return execute_motor_test(
            JsonProtocolClient(connection), motor, throttle, duration_seconds,
            progress=progress,
        )
