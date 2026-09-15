"""Download and decode the versioned raw-sector flight blackbox format."""

from __future__ import annotations

import json
import struct
import zlib
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from openflightcomputer.protocol import JsonProtocolClient, ProtocolError

SECTOR_SIZE = 512
BLOCK_MAGIC = b"OFCB"
FORMAT_VERSION = 1
SAMPLE_SIZE = 232


def select_log(logs: list[dict[str, Any]], identifier: str) -> dict[str, Any]:
    if not logs:
        raise ProtocolError("the flight computer has no flight logs")
    if identifier == "latest":
        return max(logs, key=lambda item: int(item["id"]))
    try:
        requested = int(identifier)
    except ValueError as error:
        raise ValueError("log must be 'latest' or a numeric ID") from error
    for item in logs:
        if item.get("id") == requested:
            return item
    raise ProtocolError(f"flight log {requested} does not exist")


def download_log(client: JsonProtocolClient, log: dict[str, Any], *, timeout: float) -> bytes:
    log_id = int(log["id"])
    sector_count = int(log["sector_count"])
    output = bytearray()
    for offset in range(sector_count):
        response = client.request(
            "flight_log_read",
            parameters={"log_id": log_id, "sector_offset": offset},
            timeout_seconds=timeout,
        )
        try:
            sector = bytes.fromhex(str(response["data_hex"]))
        except (KeyError, ValueError) as error:
            raise ProtocolError("flight computer returned invalid sector data") from error
        if len(sector) != SECTOR_SIZE:
            raise ProtocolError("flight computer returned a truncated sector")
        output.extend(sector)
    return bytes(output)


def _block(sector: bytes) -> tuple[int, int, int, int, bytes]:
    if len(sector) != SECTOR_SIZE or sector[:4] != BLOCK_MAGIC:
        raise ValueError("invalid blackbox block magic")
    version, block_type, log_id, sequence, item_count, payload_length = struct.unpack_from(
        "<HHIIHH", sector, 4
    )
    if version != FORMAT_VERSION or payload_length > 488:
        raise ValueError(f"unsupported blackbox block version {version}")
    expected_crc = struct.unpack_from("<I", sector, 508)[0]
    actual_crc = zlib.crc32(sector[:508]) & 0xFFFFFFFF
    if expected_crc != actual_crc:
        raise ValueError(f"blackbox block {sequence} failed CRC validation")
    return block_type, log_id, sequence, item_count, sector[20 : 20 + payload_length]


def _sample(data: bytes) -> dict[str, Any]:
    if len(data) != SAMPLE_SIZE:
        raise ValueError("invalid blackbox sample size")
    timestamp_us, imu_sequence, dt_us, events, packed_status, packed_imu, validity = struct.unpack_from(
        "<QQIIIII", data
    )
    offset = 36
    raw_acceleration = list(struct.unpack_from("<iii", data, offset)); offset += 12
    raw_gyroscope = list(struct.unpack_from("<iii", data, offset)); offset += 12
    filtered_acceleration = list(struct.unpack_from("<fff", data, offset)); offset += 12
    filtered_gyroscope = list(struct.unpack_from("<fff", data, offset)); offset += 12
    accelerometer_attitude = list(struct.unpack_from("<ff", data, offset)); offset += 8
    gyro_predicted_attitude = list(struct.unpack_from("<ff", data, offset)); offset += 8
    attitude = list(struct.unpack_from("<ff", data, offset)); offset += 8
    accelerometer_weight = struct.unpack_from("<f", data, offset)[0]; offset += 4
    receiver = list(struct.unpack_from("<ffff", data, offset)); offset += 16
    setpoint = list(struct.unpack_from("<ffff", data, offset)); offset += 16
    desired_rates = list(struct.unpack_from("<fff", data, offset)); offset += 12
    pid = [list(struct.unpack_from("<ffff", data, offset + axis * 16)) for axis in range(3)]
    offset += 48
    motors = list(struct.unpack_from("<ffff", data, offset)); offset += 16
    correction_scale, collective_shift, sequence = struct.unpack_from("<ffI", data, offset)
    return {
        "sequence": sequence, "timestamp_us": timestamp_us,
        "imu_sequence": imu_sequence, "dt_us": dt_us, "event_flags": events,
        "system_state": packed_status & 0xFF,
        "control_source": (packed_status >> 8) & 0xFF,
        "failsafe_state": (packed_status >> 16) & 0xFF,
        "failsafe_action": (packed_status >> 24) & 0xFF,
        "imu_freshness": packed_imu & 0xFF,
        "control_result": (packed_imu >> 8) & 0xFF,
        "rate_result": (packed_imu >> 16) & 0xFF,
        "validity_flags": validity,
        "raw_acceleration": raw_acceleration, "raw_gyroscope": raw_gyroscope,
        "filtered_acceleration_g": filtered_acceleration,
        "filtered_gyroscope_dps": filtered_gyroscope,
        "accelerometer_attitude_degrees": accelerometer_attitude,
        "gyro_predicted_attitude_degrees": gyro_predicted_attitude,
        "attitude_degrees": attitude, "accelerometer_weight": accelerometer_weight,
        "receiver": receiver, "setpoint": setpoint, "desired_rates_dps": desired_rates,
        "pid": pid, "motors": motors, "correction_scale": correction_scale,
        "collective_shift": collective_shift,
    }


def decode_log(data: bytes) -> dict[str, Any]:
    if not data or len(data) % SECTOR_SIZE:
        raise ValueError("blackbox file must contain complete 512-byte sectors")
    blocks = [_block(data[index : index + SECTOR_SIZE])
              for index in range(0, len(data), SECTOR_SIZE)]
    log_ids = {block[1] for block in blocks}
    if len(log_ids) != 1:
        raise ValueError("blackbox file contains mixed log IDs")
    header = next((block for block in blocks if block[0] == 1), None)
    if header is None or len(header[4]) < 68:
        raise ValueError("blackbox flight header is missing")
    sample_interval_us, config_length, config_crc, started_at_us = struct.unpack_from(
        "<IIIQ", header[4]
    )
    firmware_version = header[4][20:36].split(b"\0", 1)[0].decode("ascii", "replace")
    build_id = header[4][36:68].split(b"\0", 1)[0].decode("ascii", "replace")
    configuration = b"".join(block[4] for block in blocks if block[0] == 2)[:config_length]
    if (zlib.crc32(configuration) & 0xFFFFFFFF) != config_crc:
        raise ValueError("blackbox configuration snapshot failed CRC validation")
    samples: list[dict[str, Any]] = []
    for block_type, _log_id, _sequence, count, payload in blocks:
        if block_type == 3:
            for index in range(count):
                samples.append(_sample(payload[index * SAMPLE_SIZE : (index + 1) * SAMPLE_SIZE]))
    footer = next((block for block in reversed(blocks) if block[0] == 4), None)
    footer_data: dict[str, Any] | None = None
    if footer is not None:
        ended, captured, dropped, final_state = struct.unpack_from("<QIII", footer[4])
        footer_data = {"ended_at_us": ended, "captured_sample_count": captured,
                       "dropped_sample_count": dropped, "final_state": final_state}
    return {
        "format_version": FORMAT_VERSION, "log_id": next(iter(log_ids)),
        "sample_interval_us": sample_interval_us, "started_at_us": started_at_us,
        "firmware_version": firmware_version, "build_id": build_id,
        "configuration_snapshot_hex": configuration.hex(),
        "footer": footer_data, "samples": samples,
    }


def default_log_path(log_id: int) -> Path:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return Path(f"flight-log-v{FORMAT_VERSION}-{log_id}-{timestamp}.ofcb")


def write_decoded_json(path: Path, decoded: dict[str, Any]) -> None:
    path.write_text(json.dumps(decoded, indent=2, sort_keys=True) + "\n", encoding="utf-8")
