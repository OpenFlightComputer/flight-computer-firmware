import json
from types import SimpleNamespace

import pytest

import openflightcomputer.cli as cli
from openflightcomputer.cli import build_parser


@pytest.mark.parametrize(
    "arguments",
    [
        ["firmware", "build"],
        ["firmware", "flash"],
        ["device", "status"],
        ["device", "arm"],
        ["device", "disarm"],
        ["device", "receiver"],
        ["device", "receiver", "--watch", "--interval", "0.2"],
        ["device", "monitor"],
        ["motor", "run", "--motor", "1", "--throttle", "1.0", "--duration", "60"],
        ["motor", "direction", "show"],
        [
            "motor", "direction", "set", "--motor", "3",
            "--direction", "reversed",
        ],
        ["motor", "configuration", "reset"],
        ["smoke", "--no-flash"],
    ],
)
def test_documented_commands_parse(arguments):
    assert build_parser().parse_args(arguments).command == arguments[0]


def test_device_arm_waits_for_pending_direction_preparation(
    monkeypatch, capsys
):
    requests = []
    statuses = iter(("DISARMED", "ARMED"))

    class FakeConnectionContext:
        def __enter__(self):
            return object()

        def __exit__(self, exception_type, exception, traceback):
            return False

    class FakeClient:
        def __init__(self, connection):
            assert connection is not None

        def request(self, command, **options):
            requests.append(command)
            if command == "arm":
                return {"ok": True, "pending": True, "state": "DISARMED"}
            return {"ok": True, "state": next(statuses)}

    monkeypatch.setattr(cli, "wait_for_flight_port", lambda *args, **kwargs: object())
    monkeypatch.setattr(
        cli,
        "UsbCdcConnection",
        SimpleNamespace(open=lambda port: FakeConnectionContext()),
    )
    monkeypatch.setattr(cli, "JsonProtocolClient", FakeClient)
    monkeypatch.setattr(cli.time, "sleep", lambda duration: None)

    assert cli._device_request(
        SimpleNamespace(port=None, timeout=1.0), "arm"
    ) == 0
    assert requests == ["arm", "status", "status"]
    response = json.loads(capsys.readouterr().out)
    assert response["state"] == "ARMED"
    assert response["pending"] is False
