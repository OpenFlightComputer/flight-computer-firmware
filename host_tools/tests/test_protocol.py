import json

import pytest

from openflightcomputer.protocol import JsonProtocolClient, ProtocolError, decode_message


class FakeConnection:
    def __init__(self, lines):
        self.lines = list(lines)
        self.written = []

    def write_line(self, payload, *, timeout_seconds=2.0):
        self.written.append(payload)

    def read_line(self, *, timeout_seconds=2.0):
        return self.lines.pop(0)


def test_request_demultiplexes_log_and_correlates_response():
    connection = FakeConnection(
        [
            b'{"type":"log","message":"boot"}',
            b'{"type":"response","request_id":7,"command":"status","ok":true}',
        ]
    )
    observed = []
    response = JsonProtocolClient(connection, first_request_id=7).request(
        "status", observer=observed.append
    )
    assert response["request_id"] == 7
    assert observed[0]["type"] == "log"
    assert json.loads(connection.written[0]) == {
        "type": "command",
        "command": "status",
        "request_id": 7,
    }


def test_request_adds_command_parameters_without_replacing_envelope():
    connection = FakeConnection(
        [b'{"type":"response","request_id":1,"command":"motor_test","ok":true}']
    )
    JsonProtocolClient(connection).request(
        "motor_test", parameters={"motor": 1, "throttle": 0.02}
    )
    assert json.loads(connection.written[0]) == {
        "type": "command",
        "command": "motor_test",
        "request_id": 1,
        "motor": 1,
        "throttle": 0.02,
    }


def test_request_parameters_cannot_replace_correlated_envelope():
    with pytest.raises(ValueError, match="request_id"):
        JsonProtocolClient(FakeConnection([])).request(
            "status", parameters={"request_id": 99}
        )


def test_correlated_error_is_raised():
    connection = FakeConnection(
        [b'{"type":"error","request_id":1,"error":"unsupported_command"}']
    )
    with pytest.raises(ProtocolError, match="unsupported_command"):
        JsonProtocolClient(connection).request("future")


@pytest.mark.parametrize("line", [b"not-json", b"[]", b'{"message":"missing type"}'])
def test_invalid_messages_are_rejected(line):
    with pytest.raises(ProtocolError):
        decode_message(line)
