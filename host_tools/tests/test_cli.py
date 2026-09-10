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
        ["device", "imu"],
        ["device", "imu", "--watch", "--interval", "0.2"],
        ["device", "monitor"],
        ["motor", "run", "--motor", "1", "--throttle", "1.0", "--duration", "60"],
        ["config", "read"],
        ["config", "read", "--output", "quad.json"],
        ["config", "write", "config/default-flight-configuration.json"],
        ["config", "reset"],
        ["smoke", "--no-flash"],
    ],
)
def test_documented_commands_parse(arguments):
    assert build_parser().parse_args(arguments).command == arguments[0]


def test_imu_watch_interval_cannot_exceed_ten_requests_per_second():
    with pytest.raises(SystemExit):
        build_parser().parse_args(
            ["device", "imu", "--watch", "--interval", "0.099"]
        )


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


def test_configuration_read_can_export_portable_json(
    monkeypatch, tmp_path, capsys
):
    output = tmp_path / "quad.json"
    configuration = {
        "schema_version": 2,
        "motors": {
            "propeller_layout": "PROPS_IN",
            "directions": ["NORMAL"] * 4,
        },
    }

    class FakeConnectionContext:
        def __enter__(self):
            return object()

        def __exit__(self, exception_type, exception, traceback):
            return False

    class FakeClient:
        def __init__(self, connection):
            assert connection is not None

        def request(self, command, **options):
            assert command == "config_read"
            assert options["parameters"] is None
            return {"ok": True, "configuration": configuration}

    monkeypatch.setattr(cli, "wait_for_flight_port", lambda *args, **kwargs: object())
    monkeypatch.setattr(
        cli,
        "UsbCdcConnection",
        SimpleNamespace(open=lambda port: FakeConnectionContext()),
    )
    monkeypatch.setattr(cli, "JsonProtocolClient", FakeClient)

    assert cli._configuration_request(
        SimpleNamespace(
            configuration_command="read",
            output=output,
            port=None,
            timeout=1.0,
        )
    ) == 0
    assert json.loads(output.read_text()) == configuration
    assert str(output) in capsys.readouterr().out


def test_configuration_write_sends_file_as_one_document(
    monkeypatch, tmp_path, capsys
):
    source = tmp_path / "quad.json"
    configuration = {"schema_version": 2, "motors": {"directions": []}}
    source.write_text(json.dumps(configuration))
    observed = {}

    class FakeConnectionContext:
        def __enter__(self):
            return object()

        def __exit__(self, exception_type, exception, traceback):
            return False

    class FakeClient:
        def __init__(self, connection):
            assert connection is not None

        def request(self, command, **options):
            observed["command"] = command
            observed["parameters"] = options["parameters"]
            return {"ok": True, "configuration": configuration}

    monkeypatch.setattr(cli, "wait_for_flight_port", lambda *args, **kwargs: object())
    monkeypatch.setattr(
        cli,
        "UsbCdcConnection",
        SimpleNamespace(open=lambda port: FakeConnectionContext()),
    )
    monkeypatch.setattr(cli, "JsonProtocolClient", FakeClient)

    assert cli._configuration_request(
        SimpleNamespace(
            configuration_command="write",
            file=source,
            port=None,
            timeout=1.0,
        )
    ) == 0
    assert observed == {
        "command": "config_write",
        "parameters": {"configuration": configuration},
    }
    assert json.loads(capsys.readouterr().out) == configuration
