import struct
import zlib

import pytest

from openflightcomputer.blackbox import decode_log, select_log


def _block(block_type, sequence, payload, *, item_count=1, log_id=4):
    sector = bytearray(512)
    struct.pack_into(
        "<4sHHIIHH", sector, 0, b"OFCB", 1, block_type, log_id,
        sequence, item_count, len(payload),
    )
    sector[20 : 20 + len(payload)] = payload
    struct.pack_into("<I", sector, 508, zlib.crc32(sector[:508]) & 0xFFFFFFFF)
    return bytes(sector)


def test_decode_validates_and_exposes_flight_data():
    configuration = b"\x09configuration"
    header = bytearray(68)
    struct.pack_into(
        "<IIIQ", header, 0, 2000, len(configuration),
        zlib.crc32(configuration) & 0xFFFFFFFF, 123456,
    )
    header[20:25] = b"0.1.0"
    header[36:46] = b"build-test"
    sample = bytearray(232)
    struct.pack_into("<QQIIIII", sample, 0, 125456, 17, 1000, 0, 0, 0, 0)
    struct.pack_into("<I", sample, 228, 1)
    footer = struct.pack("<QIII", 129456, 1, 0, 2)
    raw = b"".join((
        _block(1, 0, header),
        _block(2, 1, configuration),
        _block(3, 2, sample),
        _block(4, 3, footer),
    ))

    decoded = decode_log(raw)
    assert decoded["format_version"] == 1
    assert decoded["log_id"] == 4
    assert decoded["build_id"] == "build-test"
    assert decoded["configuration_snapshot_hex"] == configuration.hex()
    assert decoded["samples"][0]["sequence"] == 1
    assert decoded["samples"][0]["timestamp_us"] == 125456
    assert decoded["footer"]["captured_sample_count"] == 1


def test_decode_rejects_corrupted_sector():
    sector = bytearray(_block(1, 0, bytes(68)))
    sector[40] ^= 1
    with pytest.raises(ValueError, match="CRC"):
        decode_log(bytes(sector))


def test_select_log_supports_latest_and_explicit_id():
    logs = [{"id": 2}, {"id": 8}, {"id": 3}]
    assert select_log(logs, "latest")["id"] == 8
    assert select_log(logs, "3")["id"] == 3
