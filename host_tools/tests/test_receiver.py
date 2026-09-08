import pytest
from rich.console import Console

from openflightcomputer.protocol import ProtocolError
from openflightcomputer.receiver import ReceiverSample, ReceiverView


def response(*, channels=None, available=True):
    return {
        "type": "response",
        "request_id": 1,
        "command": "receiver",
        "ok": True,
        "available": available,
        "sequence": 7 if available else None,
        "age_us": 1250 if available else None,
        "freshness": "FRESH" if available else "UNAVAILABLE",
        "channels": channels or ([992] * 16 if available else None),
        "normalized": {
            "roll": 0.0,
            "pitch": 0.25,
            "yaw": -0.5,
            "throttle": 0.75,
            "arm": True,
        } if available else None,
        "failsafe": {
            "state": "LIVE",
            "action": "LIVE",
            "stage_two_latched": False,
            "recovery_ready": False,
        },
        "link_statistics_present": True,
        "uplink_rssi_dbm": -42,
        "uplink_link_quality_percent": 99,
        "uplink_snr_db": 8,
        "uart_bytes": 135014,
        "valid_frames": 5000,
        "crc_errors": 0,
        "framing_errors": 0,
        "dma_overruns": 0,
        "dma_bytes_dropped": 0,
    }


def test_response_is_validated_and_converted():
    sample = ReceiverSample.from_response(response())
    assert sample.available
    assert sample.channels == (992,) * 16
    assert sample.age_us == 1250
    assert sample.pitch == 0.25
    assert sample.arm is True
    assert sample.uart_bytes == 135014


def test_view_tracks_local_minima_and_maxima():
    view = ReceiverView()
    view.update(response(channels=[900] * 16))
    view.update(response(channels=[800, 1000] + [900] * 14))
    assert view.channel_minima == [800, 900] + [900] * 14
    assert view.channel_maxima == [900, 1000] + [900] * 14


def test_render_contains_tester_and_normalized_information():
    view = ReceiverView()
    view.update(response())
    console = Console(record=True, width=120)
    console.print(view.render())
    rendered = console.export_text()
    assert "RadioMaster RP1 — CRSF receiver" in rendered
    assert "Normalized flight controls" in rendered
    assert "Receiver channels" in rendered
    assert "135014" in rendered
    assert "CRC / framing errors" in rendered
    assert "DMA overruns / dropped bytes" in rendered
    assert "CH16" in rendered


def test_unavailable_response_renders_without_snapshot():
    view = ReceiverView()
    view.update(response(available=False))
    assert view.sample is not None
    assert view.sample.channels is None
    assert view.channel_minima is None


@pytest.mark.parametrize(
    "mutation",
    [
        lambda item: item.update(available="yes"),
        lambda item: item.update(channels=[992] * 15),
        lambda item: item["normalized"].update(throttle=1.1),
        lambda item: item["failsafe"].update(recovery_ready=1),
        lambda item: item.update(dma_overruns=-1),
    ],
)
def test_invalid_receiver_responses_are_rejected(mutation):
    item = response()
    mutation(item)
    with pytest.raises(ProtocolError):
        ReceiverSample.from_response(item)
