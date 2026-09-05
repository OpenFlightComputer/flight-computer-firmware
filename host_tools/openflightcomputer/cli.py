"""Thin command-line adapter over reusable OpenFlightComputer services."""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Sequence
from datetime import datetime, timezone
from pathlib import Path

from openflightcomputer.device import DeviceError, UsbCdcConnection, wait_for_flight_port
from openflightcomputer.firmware import REPOSITORY_ROOT, FirmwareBuildError, build_firmware
from openflightcomputer.models import ProgressEvent
from openflightcomputer.programmer import ProgrammingError
from openflightcomputer.protocol import JsonProtocolClient, ProtocolError
from openflightcomputer.reporting import smoke_report_data, write_smoke_report
from openflightcomputer.workflows.flash import build_and_flash
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

    device = commands.add_parser("device", help="inspect a running flight computer")
    device_commands = device.add_subparsers(dest="device_command", required=True)
    status = device_commands.add_parser("status", help="request status and build identity")
    _add_device_options(status)
    arm = device_commands.add_parser("arm", help="request the ARMED state")
    _add_device_options(arm)
    disarm = device_commands.add_parser("disarm", help="request the DISARMED state")
    _add_device_options(disarm)
    monitor = device_commands.add_parser("monitor", help="print the live JSON stream")
    _add_device_options(monitor)

    motor = commands.add_parser("motor", help="run constrained propeller-free motor tests")
    motor_commands = motor.add_subparsers(dest="motor_command", required=True)
    motor_run = motor_commands.add_parser("run", help="run one short motor test")
    _add_device_options(motor_run)
    motor_run.add_argument("--motor", type=int, required=True, metavar="NUMBER")
    motor_run.add_argument(
        "--throttle", type=float, required=True, metavar=">0.001..0.10"
    )
    motor_run.add_argument("--duration", type=float, required=True, metavar="SECONDS")

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
        response = JsonProtocolClient(connection).request(
            command, timeout_seconds=arguments.timeout
        )
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0


def _device_monitor(arguments: argparse.Namespace) -> int:
    port = wait_for_flight_port(arguments.port, timeout_seconds=arguments.timeout)
    print(f"Monitoring {port.device}; press Ctrl-C to stop.", file=sys.stderr)
    with UsbCdcConnection.open(port) as connection:
        for message in JsonProtocolClient(connection).messages():
            print(json.dumps(message, separators=(",", ":")), flush=True)
    return 0


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
    )
    print(
        f"Completed safely: {frames} active refresh requests; motor stopped and disarmed."
    )
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
            return _device_request(arguments, arguments.device_command)
        if arguments.command == "motor":
            return _motor_run(arguments)
        return _smoke(arguments)
    except KeyboardInterrupt:
        return 130
    except (DeviceError, FirmwareBuildError, ProgrammingError, ProtocolError, ValueError) as error:
        print(f"ofc: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
