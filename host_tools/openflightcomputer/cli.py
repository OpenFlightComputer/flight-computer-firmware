"""Thin command-line adapter over reusable OpenFlightComputer services."""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections.abc import Sequence
from datetime import datetime, timezone
from pathlib import Path

from rich.console import Console
from rich.live import Live

from openflightcomputer.device import DeviceError, UsbCdcConnection, wait_for_flight_port
from openflightcomputer.control import (
    ControlTraceMetadata,
    ControlTraceView,
    export_trace,
)
from openflightcomputer.blackbox import (
    decode_log,
    default_log_path,
    download_log,
    select_log,
    write_decoded_json,
)
from openflightcomputer.firmware import REPOSITORY_ROOT, FirmwareBuildError, build_firmware
from openflightcomputer.imu import ImuView
from openflightcomputer.models import ProgressEvent
from openflightcomputer.programmer import ProgrammingError
from openflightcomputer.protocol import JsonProtocolClient, ProtocolError
from openflightcomputer.receiver import ReceiverView
from openflightcomputer.reporting import smoke_report_data, write_smoke_report
from openflightcomputer.workflows.flash import build_and_flash
from openflightcomputer.workflows.usb_flash import build_and_flash_usb
from openflightcomputer.workflows.motor import run_motor_test
from openflightcomputer.workflows.smoke import run_smoke


def _existing_file(value: str) -> Path:
    path = Path(value).expanduser()
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file does not exist: {value}")
    return path


def _firmware_file(value: str) -> Path:
    path = _existing_file(value)
    if path.suffix.lower() != ".elf":
        raise argparse.ArgumentTypeError(f"firmware must be an ELF file: {value}")
    return path


def _positive_float(value: str) -> float:
    parsed = float(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return parsed


def _inspection_interval(value: str) -> float:
    parsed = _positive_float(value)
    if parsed < 0.1:
        raise argparse.ArgumentTypeError("inspection interval must be at least 0.1 seconds")
    return parsed


def _add_profile(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--profile", choices=("debug", "release"), default="release",
        help="firmware profile (default: release)",
    )


def _add_flash_options(parser: argparse.ArgumentParser) -> None:
    _add_profile(parser)
    parser.add_argument("--firmware", type=_firmware_file, metavar="ELF")
    parser.add_argument("--probe-serial", metavar="SERIAL")
    parser.add_argument("--programmer", type=_existing_file, metavar="PATH")


def _add_device_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--port", metavar="PATH")
    parser.add_argument("--timeout", type=_positive_float, default=10.0, metavar="SECONDS")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ofc",
        description="Build, flash, inspect, and smoke-test OpenFlightComputer firmware",
    )
    commands = parser.add_subparsers(dest="command", required=True)

    firmware = commands.add_parser("firmware", help="build or flash firmware")
    firmware_commands = firmware.add_subparsers(dest="firmware_command", required=True)
    build = firmware_commands.add_parser("build", help="build a firmware image")
    _add_profile(build)
    flash = firmware_commands.add_parser("flash", help="build, program, verify, and reset")
    _add_flash_options(flash)
    flash_usb = firmware_commands.add_parser(
        "flash-usb", help="build and update through the resident USB bootloader"
    )
    _add_profile(flash_usb)
    flash_usb.add_argument("--firmware", type=_firmware_file, metavar="ELF")
    flash_usb.add_argument("--programmer", type=_existing_file, metavar="PATH")
    _add_device_options(flash_usb)
    flash_usb.set_defaults(timeout=20.0)

    device = commands.add_parser("device", help="inspect a running flight computer")
    device_commands = device.add_subparsers(dest="device_command", required=True)
    status = device_commands.add_parser("status", help="request status and build identity")
    _add_device_options(status)
    arm = device_commands.add_parser("arm", help="request the ARMED state")
    _add_device_options(arm)
    disarm = device_commands.add_parser("disarm", help="request the DISARMED state")
    _add_device_options(disarm)
    receiver = device_commands.add_parser(
        "receiver", help="inspect raw and normalized receiver input"
    )
    _add_device_options(receiver)
    receiver.add_argument(
        "--watch", action="store_true", help="continuously refresh the receiver view"
    )
    imu = device_commands.add_parser(
        "imu", help="inspect the latest mapped BMI270 sample"
    )
    imu.add_argument(
        "imu_action", nargs="?", choices=("calibrate-level",),
        help="calibrate and persist the mounted flight computer's level attitude",
    )
    _add_device_options(imu)
    imu.add_argument(
        "--watch", action="store_true", help="continuously refresh the IMU view"
    )
    control = device_commands.add_parser(
        "control", help="inspect attitude, rate control, mixing, and motor output"
    )
    _add_device_options(control)
    control.add_argument(
        "--watch", action="store_true", help="start a trace and continuously refresh"
    )
    control.add_argument(
        "--level", choices=("events", "low", "high", "full"), default="high",
        help="trace granularity for --watch (default: high, 100 Hz)",
    )
    control.add_argument(
        "--interval", type=_inspection_interval, default=0.1, metavar="SECONDS",
        help="USB polling interval, minimum 0.1 (default: 0.1)",
    )
    control.add_argument(
        "--output", type=Path, metavar="JSON_OR_CSV",
        help="export all records with a unique versioned filename and metadata",
    )
    imu.add_argument(
        "--interval",
        type=_inspection_interval,
        default=0.1,
        metavar="SECONDS",
        help="watch polling interval, minimum 0.1 (default: 0.1)",
    )
    receiver.add_argument(
        "--interval",
        type=_positive_float,
        default=0.1,
        metavar="SECONDS",
        help="watch polling interval (default: 0.1)",
    )
    monitor = device_commands.add_parser("monitor", help="print the live JSON stream")
    _add_device_options(monitor)

    motor = commands.add_parser("motor", help="run constrained propeller-free motor tests")
    motor_commands = motor.add_subparsers(dest="motor_command", required=True)
    motor_run = motor_commands.add_parser("run", help="run one short motor test")
    _add_device_options(motor_run)
    motor_run.add_argument("--motor", type=int, choices=range(1, 5), required=True)
    motor_run.add_argument(
        "--throttle", type=float, required=True, metavar=">0.001..1.0"
    )
    motor_run.add_argument("--duration", type=float, required=True, metavar="SECONDS")
    configuration = commands.add_parser(
        "config", help="read or replace the complete flight configuration"
    )
    configuration_commands = configuration.add_subparsers(
        dest="configuration_command", required=True
    )
    configuration_read = configuration_commands.add_parser(
        "read", help="read the complete active configuration"
    )
    _add_device_options(configuration_read)
    configuration_read.add_argument("--output", type=Path, metavar="PATH")
    configuration_write = configuration_commands.add_parser(
        "write", help="validate and write a complete JSON configuration"
    )
    _add_device_options(configuration_write)
    configuration_write.add_argument("file", type=_existing_file, metavar="JSON")
    configuration_reset = configuration_commands.add_parser(
        "reset", help="erase the override and restore compiled JSON defaults"
    )
    _add_device_options(configuration_reset)

    storage = commands.add_parser("storage", help="inspect or initialize blackbox storage")
    storage_commands = storage.add_subparsers(dest="storage_command", required=True)
    storage_status = storage_commands.add_parser("status", help="show SD and blackbox status")
    _add_device_options(storage_status)
    storage_initialize = storage_commands.add_parser(
        "initialize", help="erase the SD card's raw blackbox index"
    )
    _add_device_options(storage_initialize)
    storage_initialize.add_argument(
        "--yes", action="store_true", help="confirm erasing every existing blackbox log"
    )

    flight_log = commands.add_parser("flight-log", help="list, download, or decode flight logs")
    flight_log_commands = flight_log.add_subparsers(dest="flight_log_command", required=True)
    flight_log_list = flight_log_commands.add_parser("list", help="list logs stored on the SD card")
    _add_device_options(flight_log_list)
    flight_log_download = flight_log_commands.add_parser(
        "download", help="download one log by ID or use 'latest'"
    )
    _add_device_options(flight_log_download)
    flight_log_download.add_argument("log", help="numeric log ID or 'latest'")
    flight_log_download.add_argument("--output", type=Path, metavar="OFCB")
    flight_log_download.add_argument(
        "--json-output", type=Path, metavar="JSON", help="also decode into inspectable JSON"
    )
    flight_log_decode = flight_log_commands.add_parser(
        "decode", help="decode an already downloaded .ofcb file"
    )
    flight_log_decode.add_argument("file", type=_existing_file, metavar="OFCB")
    flight_log_decode.add_argument("--output", type=Path, metavar="JSON")

    smoke = commands.add_parser(
        "smoke", help="optionally flash, then run non-arming status and health checks"
    )
    _add_flash_options(smoke)
    _add_device_options(smoke)
    smoke.add_argument(
        "--no-flash", action="store_true", help="test the already-running image"
    )
    smoke.add_argument("--report", type=Path, metavar="PATH")
    smoke.add_argument("--json", action="store_true", help="print the result as JSON")
    return parser


def _progress(event: ProgressEvent) -> None:
    print(f"[{event.operation}] {event.message}", flush=True)


def _default_report_path() -> Path:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return REPOSITORY_ROOT / "reports" / f"smoke-{timestamp}.json"


def _device_request(arguments: argparse.Namespace, command: str) -> int:
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    with UsbCdcConnection.open(port) as connection:
        client = JsonProtocolClient(connection)
        response = client.request(
            command, timeout_seconds=arguments.timeout
        )
        if command == "arm" and response.get("pending") is True:
            deadline = time.monotonic() + arguments.timeout
            while response.get("state") != "ARMED":
                if time.monotonic() >= deadline:
                    raise ProtocolError("timed out waiting for arm preparation")
                status = client.request(
                    "status", timeout_seconds=max(0.01, deadline - time.monotonic())
                )
                if status.get("state") == "FAULT":
                    raise ProtocolError("arm preparation entered FAULT")
                response["state"] = status.get("state")
                if response["state"] != "ARMED":
                    time.sleep(0.01)
            response["pending"] = False
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _device_monitor(arguments: argparse.Namespace) -> int:
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    print(f"Monitoring {port.device}; press Ctrl-C to stop.", file=sys.stderr)
    with UsbCdcConnection.open(port) as connection:
        for message in JsonProtocolClient(connection).messages():
            print(json.dumps(message, separators=(",", ":")), flush=True)
    return 0


def _device_receiver(arguments: argparse.Namespace) -> int:
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    view = ReceiverView()
    console = Console()
    with UsbCdcConnection.open(port) as connection:
        client = JsonProtocolClient(connection)
        if not arguments.watch:
            view.update(client.request("receiver", timeout_seconds=arguments.timeout))
            console.print(view.render())
            return 0

        print(
            f"Watching receiver on {port.device}; press Ctrl-C to stop.",
            file=sys.stderr,
        )
        with Live(
            view.render(),
            console=console,
            refresh_per_second=10,
        ) as live:
            while True:
                view.update(
                    client.request("receiver", timeout_seconds=arguments.timeout)
                )
                live.update(view.render(), refresh=True)
                time.sleep(arguments.interval)


def _device_imu(arguments: argparse.Namespace) -> int:
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    view = ImuView()
    console = Console()
    with UsbCdcConnection.open(port) as connection:
        client = JsonProtocolClient(connection)
        if arguments.imu_action == "calibrate-level":
            client.request(
                "imu_level_calibration_start",
                timeout_seconds=arguments.timeout,
            )
            print(
                "Keep the flight computer level and completely still while it calibrates.",
                file=sys.stderr,
            )
            deadline = time.monotonic() + arguments.timeout
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0.0:
                    raise ProtocolError("timed out waiting for level calibration")
                response = client.request(
                    "imu", timeout_seconds=max(0.01, remaining)
                )
                view.update(response)
                state = view.sample.level_calibration_state
                if state == "READY":
                    console.print(view.render())
                    print("Level calibration saved.")
                    return 0
                if state not in {"COLLECTING", "SAVING"}:
                    raise ProtocolError(
                        f"level calibration failed: {state.lower()}"
                    )
                time.sleep(min(arguments.interval, max(0.0, remaining)))
        if not arguments.watch:
            view.update(client.request("imu", timeout_seconds=arguments.timeout))
            console.print(view.render())
            return 0

        print(f"Watching IMU on {port.device}; press Ctrl-C to stop.", file=sys.stderr)
        with Live(view.render(), console=console, refresh_per_second=10) as live:
            while True:
                view.update(client.request("imu", timeout_seconds=arguments.timeout))
                live.update(view.render(), refresh=True)
                time.sleep(arguments.interval)


def _device_control(arguments: argparse.Namespace) -> int:
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    view = ControlTraceView()
    console = Console()
    started = False
    interrupted = False
    status_response: dict[str, object] = {}
    configuration_response: dict[str, object] = {}
    with UsbCdcConnection.open(port) as connection:
        client = JsonProtocolClient(connection)
        try:
            if arguments.watch or (arguments.output is not None):
                status_response = client.request(
                    "status", timeout_seconds=arguments.timeout
                )
                configuration_response = client.request(
                    "config_read", timeout_seconds=arguments.timeout
                )
            if arguments.watch:
                level = {
                    "events": "EVENTS", "low": "LOW_RATE",
                    "high": "HIGH_RATE", "full": "FULL_RATE",
                }[arguments.level]
                client.request(
                    "control_trace_start",
                    parameters={"level": level},
                    timeout_seconds=arguments.timeout,
                )
                started = True
                print(
                    f"Watching control trace on {port.device}; press Ctrl-C to stop.",
                    file=sys.stderr,
                )
                with Live(view.render(), console=console, refresh_per_second=10) as live:
                    while True:
                        batch = view.update(client.request(
                            "control_trace_read", timeout_seconds=arguments.timeout
                        ))
                        live.update(view.render(), refresh=True)
                        if not batch.capturing and batch.pending_records <= len(batch.records):
                            break
                        if batch.pending_records <= len(batch.records):
                            time.sleep(arguments.interval)
            else:
                view.update(client.request(
                    "control_trace_read", timeout_seconds=arguments.timeout
                ))
                console.print(view.render())
        except KeyboardInterrupt:
            interrupted = True
        finally:
            if started:
                try:
                    client.request("control_trace_stop", timeout_seconds=arguments.timeout)
                except ProtocolError:
                    pass
                else:
                    try:
                        while True:
                            batch = view.update(client.request(
                                "control_trace_read",
                                timeout_seconds=arguments.timeout,
                            ))
                            if batch.pending_records <= len(batch.records):
                                break
                    except ProtocolError:
                        pass
    output = arguments.output
    if arguments.watch and output is None:
        output = Path("control-trace.json")
    if output is not None:
        metadata = ControlTraceMetadata.create(
            status_response, configuration_response, view
        )
        output_path = export_trace(output, view.records, metadata)
        print(f"Control trace: {output_path}")
    if view.dropped_records:
        print(f"Warning: firmware dropped {view.dropped_records} trace records.", file=sys.stderr)
    return 130 if interrupted else 0


def _smoke(arguments: argparse.Namespace) -> int:
    progress = (lambda _event: None) if arguments.json else _progress
    result = run_smoke(
        arguments.profile,
        flash=not arguments.no_flash,
        firmware_path=arguments.firmware,
        probe_serial=arguments.probe_serial,
        programmer_path=arguments.programmer,
        requested_port=arguments.port,
        timeout_seconds=arguments.timeout,
        progress=progress,
    )
    report_path = write_smoke_report(result, arguments.report or _default_report_path())
    if arguments.json:
        print(json.dumps(smoke_report_data(result), indent=2, sort_keys=True))
    else:
        for check in result.checks:
            print(f"{'PASS' if check.passed else 'FAIL'} {check.name}: {check.detail}")
        print(f"Report: {report_path}")
    return 0 if result.passed else 2


def _motor_run(arguments: argparse.Namespace) -> int:
    print(
        f"Running motor {arguments.motor} at {arguments.throttle:.1%} for "
        f"{arguments.duration:g}s; Ctrl-C triggers stop and disarm.",
        file=sys.stderr,
    )
    frames = run_motor_test(
        arguments.motor,
        arguments.throttle,
        arguments.duration,
        requested_port=arguments.port,
        timeout_seconds=arguments.timeout,
        progress=lambda message: print(message, file=sys.stderr, flush=True),
    )
    print(
        f"Completed safely: {frames} active refresh requests; motor stopped and disarmed."
    )
    return 0


def _configuration_request(arguments: argparse.Namespace) -> int:
    command = f"config_{arguments.configuration_command}"
    parameters = None
    if arguments.configuration_command == "write":
        try:
            configuration = json.loads(arguments.file.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            raise ValueError(f"cannot read configuration JSON: {error}") from error
        if not isinstance(configuration, dict):
            raise ValueError("configuration JSON must contain one object")
        parameters = {"configuration": configuration}

    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    with UsbCdcConnection.open(port) as connection:
        response = JsonProtocolClient(connection).request(
            command,
            parameters=parameters,
            timeout_seconds=arguments.timeout,
        )
    configuration = response.get("configuration")
    if not isinstance(configuration, dict):
        raise ProtocolError("configuration response does not contain an object")
    rendered = json.dumps(configuration, indent=2, sort_keys=True) + "\n"
    if arguments.configuration_command == "read" and arguments.output is not None:
        arguments.output.write_text(rendered, encoding="utf-8")
        print(f"Configuration: {arguments.output}")
    else:
        print(rendered, end="")
    return 0


def _storage_request(arguments: argparse.Namespace) -> int:
    if arguments.storage_command == "initialize" and not arguments.yes:
        raise ValueError("storage initialize erases the log index; pass --yes to confirm")
    command = f"storage_{arguments.storage_command}"
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    with UsbCdcConnection.open(port) as connection:
        response = JsonProtocolClient(connection).request(
            command, timeout_seconds=arguments.timeout
        )
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _flight_log_request(arguments: argparse.Namespace) -> int:
    if arguments.flight_log_command == "decode":
        decoded = decode_log(arguments.file.read_bytes())
        output = arguments.output or arguments.file.with_suffix(".json")
        write_decoded_json(output, decoded)
        print(f"Decoded flight log: {output}")
        return 0

    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    with UsbCdcConnection.open(port) as connection:
        client = JsonProtocolClient(connection)
        response = client.request("flight_log_list", timeout_seconds=arguments.timeout)
        logs = response.get("logs")
        if not isinstance(logs, list):
            raise ProtocolError("flight computer returned an invalid log list")
        if arguments.flight_log_command == "list":
            print(json.dumps(response, indent=2, sort_keys=True))
            return 0
        log = select_log(logs, arguments.log)
        data = download_log(client, log, timeout=arguments.timeout)

    output = arguments.output or default_log_path(int(log["id"]))
    output.write_bytes(data)
    print(f"Flight log: {output}")
    if arguments.json_output is not None:
        write_decoded_json(arguments.json_output, decode_log(data))
        print(f"Decoded flight log: {arguments.json_output}")
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        if arguments.command == "firmware":
            if arguments.firmware_command == "build":
                artifact = build_firmware(arguments.profile, progress=_progress)
                print(f"Firmware: {artifact.elf_path}")
                print(f"Version: {artifact.firmware_version or 'unknown'}")
                print(f"Build ID: {artifact.build_id or 'unknown'}")
                return 0
            if arguments.firmware_command == "flash-usb":
                outcome = build_and_flash_usb(
                    arguments.profile,
                    firmware_path=arguments.firmware,
                    programmer_path=arguments.programmer,
                    requested_port=arguments.port,
                    timeout_seconds=arguments.timeout,
                    progress=_progress,
                )
                print(f"Firmware: {outcome.artifact.elf_path}")
                print(f"Bootloader port: {outcome.bootloader_port}")
                print(f"Device: {outcome.device_port}")
                print(f"Build ID: {outcome.status.get('build_id', 'unknown')}")
                return 0
            outcome = build_and_flash(
                arguments.profile,
                firmware_path=arguments.firmware,
                probe_serial=arguments.probe_serial,
                programmer_path=arguments.programmer,
                progress=_progress,
            )
            print(f"Firmware: {outcome.artifact.elf_path}")
            print(f"ST-Link: {outcome.probe.serial_number}")
            return 0
        if arguments.command == "device":
            if arguments.device_command == "monitor":
                return _device_monitor(arguments)
            if arguments.device_command == "receiver":
                return _device_receiver(arguments)
            if arguments.device_command == "imu":
                return _device_imu(arguments)
            if arguments.device_command == "control":
                return _device_control(arguments)
            return _device_request(arguments, arguments.device_command)
        if arguments.command == "motor":
            return _motor_run(arguments)
        if arguments.command == "config":
            return _configuration_request(arguments)
        if arguments.command == "storage":
            return _storage_request(arguments)
        if arguments.command == "flight-log":
            return _flight_log_request(arguments)
        return _smoke(arguments)
    except KeyboardInterrupt:
        return 130
    except (DeviceError, FirmwareBuildError, ProgrammingError, ProtocolError, ValueError) as error:
        print(f"ofc: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
