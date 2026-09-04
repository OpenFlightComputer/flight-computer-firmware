"""USB CDC discovery, connection ownership, and bounded line I/O."""

from __future__ import annotations

import time
from collections.abc import Callable, Iterable
from pathlib import Path
from typing import Protocol

import serial
from serial.tools import list_ports

from openflightcomputer.models import SerialPort


FLIGHT_USB_VID = 0xCAFE
FLIGHT_USB_PID = 0x4002
SERIAL_BAUD_RATE = 115200
SERIAL_READ_SLICE_SECONDS = 0.1
MAXIMUM_LINE_BYTES = 4096


class DeviceError(RuntimeError):
    pass


class DeviceTimeoutError(DeviceError):
    pass


class SerialDevice(Protocol):
    timeout: float | None
    write_timeout: float | None
    def read(self, size: int = 1) -> bytes: ...
    def write(self, data: bytes | memoryview) -> int | None: ...
    def close(self) -> None: ...


PortLister = Callable[[], Iterable[SerialPort]]
SerialFactory = Callable[..., SerialDevice]


def list_available_ports() -> tuple[SerialPort, ...]:
    return tuple(
        sorted(
            (
                SerialPort(
                    device=port.device,
                    vid=port.vid,
                    pid=port.pid,
                    description=port.description,
                    serial_number=port.serial_number,
                )
                for port in list_ports.comports()
            ),
            key=lambda item: item.device,
        )
    )


def wait_for_flight_port(
    requested_port: str | Path | None = None,
    *,
    timeout_seconds: float = 10.0,
    poll_seconds: float = 0.25,
    port_lister: PortLister = list_available_ports,
    monotonic: Callable[[], float] = time.monotonic,
    sleeper: Callable[[float], None] = time.sleep,
) -> SerialPort:
    if timeout_seconds < 0 or poll_seconds <= 0:
        raise ValueError("timeout must be non-negative and poll interval positive")
    requested = str(requested_port) if requested_port is not None else None
    deadline = monotonic() + timeout_seconds
    while True:
        try:
            ports = tuple(port_lister())
        except (serial.SerialException, OSError) as error:
            raise DeviceError(f"could not enumerate USB CDC ports: {error}") from error
        matches = tuple(
            port
            for port in ports
            if (port.device == requested if requested is not None else
                port.vid == FLIGHT_USB_VID and port.pid == FLIGHT_USB_PID)
        )
        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            devices = ", ".join(port.device for port in matches)
            raise DeviceError(f"multiple matching flight computers found ({devices}); use --port")
        now = monotonic()
        if now >= deadline:
            selector = requested or f"VID:PID {FLIGHT_USB_VID:04X}:{FLIGHT_USB_PID:04X}"
            raise DeviceError(
                f"flight computer {selector} did not appear within "
                f"{timeout_seconds:g}s"
            )
        sleeper(min(poll_seconds, deadline - now))


class UsbCdcConnection:
    def __init__(self, port: SerialPort, device: SerialDevice) -> None:
        self.port = port
        self._device = device
        self._receive = bytearray()
        self._closed = False

    @classmethod
    def open(
        cls,
        port: SerialPort,
        *,
        serial_factory: SerialFactory = serial.Serial,
    ) -> "UsbCdcConnection":
        try:
            device = serial_factory(
                port=port.device,
                baudrate=SERIAL_BAUD_RATE,
                timeout=SERIAL_READ_SLICE_SECONDS,
                write_timeout=SERIAL_READ_SLICE_SECONDS,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False,
            )
        except (serial.SerialException, OSError) as error:
            raise DeviceError(f"could not open {port.device}: {error}") from error
        return cls(port, device)

    def __enter__(self) -> "UsbCdcConnection":
        return self

    def __exit__(self, *_error: object) -> None:
        self.close()

    def close(self) -> None:
        if self._closed:
            return
        try:
            self._device.close()
        except (serial.SerialException, OSError) as error:
            raise DeviceError(f"could not close {self.port.device}: {error}") from error
        self._closed = True

    def write_line(self, payload: bytes, *, timeout_seconds: float = 2.0) -> None:
        frame = payload + b"\n"
        offset = 0
        deadline = time.monotonic() + timeout_seconds
        while offset < len(frame):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise DeviceTimeoutError("USB write timed out")
            self._device.write_timeout = remaining
            try:
                written = self._device.write(memoryview(frame)[offset:])
            except serial.SerialTimeoutException as error:
                raise DeviceTimeoutError("USB write timed out") from error
            except (serial.SerialException, OSError) as error:
                raise DeviceError(f"USB write failed: {error}") from error
            if written is None or written <= 0:
                raise DeviceError("USB device accepted no output bytes")
            offset += written

    def read_line(self, *, timeout_seconds: float = 2.0) -> bytes:
        deadline = time.monotonic() + timeout_seconds
        while True:
            newline = self._receive.find(b"\n")
            if newline >= 0:
                line = bytes(self._receive[:newline])
                del self._receive[: newline + 1]
                return line[:-1] if line.endswith(b"\r") else line
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise DeviceTimeoutError("USB read timed out")
            self._device.timeout = min(SERIAL_READ_SLICE_SECONDS, remaining)
            try:
                chunk = self._device.read(512)
            except (serial.SerialException, OSError) as error:
                raise DeviceError(f"USB read failed: {error}") from error
            if chunk:
                self._receive.extend(chunk)
                if len(self._receive) > MAXIMUM_LINE_BYTES:
                    self._receive.clear()
                    raise DeviceError("USB input exceeded the host line limit")
