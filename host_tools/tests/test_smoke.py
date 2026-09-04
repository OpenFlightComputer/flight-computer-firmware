import json
from pathlib import Path

from openflightcomputer.models import FirmwareArtifact, FlashOutcome, Probe, SerialPort
from openflightcomputer.reporting import smoke_report_data, write_smoke_report
from openflightcomputer.workflows.smoke import run_smoke


class FakeConnection:
    def __init__(self, port):
        self.port = port
        self.requests = []
        self.responses = [
            b'{"type":"log","message":"running"}',
            b'{"type":"response","request_id":1,"command":"status","ok":true,'
            b'"state":"DISARMED","firmware_version":"0.1.0","build_id":"abc1234"}',
            b'{"type":"response","request_id":2,"command":"health","ok":true,'
            b'"health":"OK","fault_data_complete":true,"dropped_fault_count":0}',
        ]

    def __enter__(self):
        return self

    def __exit__(self, *_error):
        pass

    def write_line(self, payload, *, timeout_seconds=2.0):
        self.requests.append(json.loads(payload))

    def read_line(self, *, timeout_seconds=2.0):
        return self.responses.pop(0)


def test_smoke_flashes_and_never_arms(tmp_path: Path):
    artifact = FirmwareArtifact("release", tmp_path / "flight.elf", "0.1.0", "abc1234")
    outcome = FlashOutcome(artifact, Probe("probe"))
    connection = FakeConnection(SerialPort("port", 0xCAFE, 0x4002))

    result = run_smoke(
        flash_workflow=lambda *args, **kwargs: outcome,
        port_waiter=lambda *args, **kwargs: SerialPort("port", 0xCAFE, 0x4002),
        connection_factory=lambda _port: connection,
    )
    assert result.passed
    assert next(check for check in result.checks if check.name == "flashed_identity").passed
    assert [request["command"] for request in connection.requests] == ["status", "health"]
    assert len(result.observed_logs) == 1
    assert result.artifact == artifact

    report = write_smoke_report(result, tmp_path / "report.json")
    loaded = json.loads(report.read_text())
    assert loaded["passed"] is True
    assert loaded["artifact"]["elf_path"] == str(artifact.elf_path)
    assert smoke_report_data(result)["probe"]["serial_number"] == "probe"
