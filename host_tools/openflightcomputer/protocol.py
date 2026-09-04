"""Request correlation and JSON/log demultiplexing for flight firmware."""

from __future__ import annotations

import json
import time
from collections.abc import Callable, Iterator
from typing import Any, Protocol


class ProtocolError(RuntimeError):
    pass


class LineConnection(Protocol):
    def write_line(self, payload: bytes, *, timeout_seconds: float = 2.0) -> None: ...
    def read_line(self, *, timeout_seconds: float = 2.0) -> bytes: ...


MessageObserver = Callable[[dict[str, Any]], None]


def decode_message(line: bytes) -> dict[str, Any]:
    try:
        value = json.loads(line)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ProtocolError(f"device sent invalid JSON: {line!r}") from error
    if not isinstance(value, dict) or not isinstance(value.get("type"), str):
        raise ProtocolError("device JSON is not a typed object")
    return value


class JsonProtocolClient:
    def __init__(self, connection: LineConnection, *, first_request_id: int = 1) -> None:
        if not 0 <= first_request_id <= 0xFFFFFFFF:
            raise ValueError("request ID must fit uint32")
        self._connection = connection
        self._next_request_id = first_request_id

    def request(
        self,
        command: str,
        *,
        timeout_seconds: float = 2.0,
        observer: MessageObserver | None = None,
    ) -> dict[str, Any]:
        request_id = self._next_request_id
        self._next_request_id = (request_id + 1) & 0xFFFFFFFF
        payload = json.dumps(
            {"type": "command", "command": command, "request_id": request_id},
            separators=(",", ":"),
        ).encode("utf-8")
        self._connection.write_line(payload, timeout_seconds=timeout_seconds)
        deadline = time.monotonic() + timeout_seconds
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise ProtocolError(f"timed out waiting for {command} response")
            message = decode_message(
                self._connection.read_line(timeout_seconds=remaining)
            )
            if observer is not None:
                observer(message)
            message_id = message.get("request_id")
            if type(message_id) is int and message_id == request_id:
                if message.get("type") == "error" or message.get("ok") is False:
                    raise ProtocolError(
                        f"{command} request failed: {message.get('error', 'unknown error')}"
                    )
                if message.get("type") != "response" or message.get("command") != command:
                    raise ProtocolError(f"unexpected correlated response: {message}")
                return message

    def messages(self, *, timeout_seconds: float = 3600.0) -> Iterator[dict[str, Any]]:
        while True:
            yield decode_message(self._connection.read_line(timeout_seconds=timeout_seconds))
