import json

import pytest
from rich.console import Console

from openflightcomputer.control import ControlTraceBatch, ControlTraceView, export_trace
from openflightcomputer.protocol import ProtocolError


def response(records=None):
    return {
        "type": "response",
        "request_id": 1,
        "command": "control_trace_read",
        "ok": True,
        "schema_version": 1,
        "capture_id": 3,
        "level": "HIGH_RATE",
        "capturing": True,
        "first_sequence": 1,
        "next_sequence": 2,
        "pending_records": 1,
        "dropped_records": 0,
        "scale": 1000,
        "records": records if records is not None else [[
            1, 123456, 40, 32, 3, 2, 1, 1, 1, 1, 0, 31, 1000,
            [500, 100, -200, 50], [450, 3000, -6000, 7500], [2500, -4000],
            [12000, -24000, 7500], [10000, -20000, 7000],
            [4, 1, 0, 5, -8, 2, 1, -5, 1, 0, 0, 1],
            [410, 470, 430, 490], 850, -20, 1,
        ]],
    }


def test_control_trace_decodes_scaled_compact_record():
    batch = ControlTraceBatch.from_response(response())
    record = batch.records[0]
    assert record.state == "ARMED"
    assert record.source == "RECEIVER"
    assert record.receiver == (0.5, 0.1, -0.2, 0.05)
    assert record.attitude == (2.5, -4.0)
    assert record.pid[1] == (-0.008, 0.002, 0.001, -0.005)
    assert record.motors == (0.41, 0.47, 0.43, 0.49)
    assert record.mixer_saturated is True


@pytest.mark.parametrize(
    "mutation",
    [
        lambda data: data.update(schema_version=2),
        lambda data: data.update(scale=0),
        lambda data: data.update(records=[[1, 2, 3]]),
        lambda data: data["records"][0].__setitem__(22, 2),
    ],
)
def test_control_trace_rejects_incompatible_wire_data(mutation):
    data = response()
    mutation(data)
    with pytest.raises(ProtocolError):
        ControlTraceBatch.from_response(data)


def test_view_renders_attitude_motors_pid_and_history():
    view = ControlTraceView()
    view.update(response())
    console = Console(record=True, width=150)
    console.print(view.render())
    text = console.export_text()
    assert "Flight-control trace" in text
    assert "roll   +2.5°" in text
    assert "M1 front-left  41.0%" in text
    assert "Rate controller" in text
    assert "saturated YES" in text


def test_trace_exports_json_and_csv(tmp_path):
    view = ControlTraceView()
    view.update(response())
    json_path = tmp_path / "trace.json"
    csv_path = tmp_path / "trace.csv"
    export_trace(json_path, view.records)
    export_trace(csv_path, view.records)
    assert json.loads(json_path.read_text())[0]["sequence"] == 1
    assert "sequence,timestamp_us" in csv_path.read_text()
    assert "[0.5, 0.1, -0.2, 0.05]" in csv_path.read_text()
