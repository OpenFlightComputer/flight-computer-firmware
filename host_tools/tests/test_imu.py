import pytest

from openflightcomputer.imu import ImuSample, ImuView
from openflightcomputer.protocol import ProtocolError


def response() -> dict:
    return {
        "type": "response",
        "request_id": 1,
        "command": "imu",
        "ok": True,
        "available": True,
        "sequence": 7,
        "age_us": 250,
        "freshness": "FRESH",
        "acceleration_raw": {"x": 16384, "y": -8192, "z": 0},
        "gyroscope_raw": {"x": 16384, "y": -4096, "z": 0},
        "gyroscope_corrected_raw": {"x": 16380, "y": -4090, "z": -2},
        "calibration": {
            "state": "READY",
            "progress_permille": 1000,
            "samples": 500,
            "restarts": 1,
            "bias_raw": {"x": 4, "y": -6, "z": 2},
        },
        "service": {"reads": 10, "published": 9, "source_errors": 1},
        "task": {
            "executions": 11,
            "last_execution_us": 12,
            "maximum_execution_us": 13,
            "overruns": 0,
            "missed_releases": 2,
        },
        "high_rate": {"budget_us": 100, "utilization_permille": 100},
    }


def test_sample_converts_independent_acceleration_and_gyro_scales():
    sample = ImuSample.from_response(response())

    assert sample.acceleration_g == pytest.approx((1.0, -0.5, 0.0))
    assert sample.gyroscope_dps == pytest.approx((1000.0, -250.0, 0.0))
    assert sample.task_maximum_execution_us == 13
    assert sample.calibration_bias_dps == pytest.approx((0.244140625, -0.3662109375, 0.1220703125))


def test_unavailable_response_accepts_only_null_snapshot_fields():
    data = response()
    data.update(
        available=False,
        sequence=None,
        age_us=None,
        acceleration_raw=None,
        gyroscope_raw=None,
        gyroscope_corrected_raw=None,
        task=None,
        freshness="UNAVAILABLE",
    )
    data["calibration"] = {
        "state": "SETTLING",
        "progress_permille": 0,
        "samples": 0,
        "restarts": 0,
        "bias_raw": None,
    }
    sample = ImuSample.from_response(data)
    assert sample.acceleration_g is None

    data["sequence"] = 1
    with pytest.raises(ProtocolError):
        ImuSample.from_response(data)


def test_view_renders_body_axis_convention_and_scheduler_health():
    view = ImuView()
    view.update(response())
    rendered = "\n".join(str(item) for item in view.render().renderables)

    assert view.sample is not None
    assert view.sample.freshness == "FRESH"
    assert len(view.render().renderables) == 3


@pytest.mark.parametrize(
    "mutation",
    [
        {"command": "receiver"},
        {"freshness": "INVALID"},
        {"available": "yes"},
        {"task": None},
    ],
)
def test_invalid_responses_are_rejected(mutation):
    data = response()
    data.update(mutation)
    with pytest.raises(ProtocolError):
        ImuSample.from_response(data)
