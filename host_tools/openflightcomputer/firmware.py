"""Build flight firmware through the repository CMake presets."""

from __future__ import annotations

import re
import shutil
import struct
import zlib
from collections.abc import Callable
from pathlib import Path

from openflightcomputer.external_tools import (
    CommandRunner,
    ExternalCommandError,
    failure_detail,
    run_command,
)
from openflightcomputer.models import FirmwareArtifact, FirmwareProfile, ProgressEvent


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ProgressCallback = Callable[[ProgressEvent], None]
ExecutableLocator = Callable[[str], str | None]


class FirmwareBuildError(RuntimeError):
    pass


def _no_progress(_event: ProgressEvent) -> None:
    pass


def _read_generated_identity(path: Path) -> tuple[str | None, str | None]:
    try:
        source = path.read_text(encoding="utf-8")
    except OSError:
        return None, None
    version = re.search(r'firmware_version\[\] = "([^"]+)";', source)
    build_id = re.search(r'firmware_build_id\[\] = "([^"]+)";', source)
    return (
        version.group(1) if version else None,
        build_id.group(1) if build_id else None,
    )


def build_firmware(
    profile: FirmwareProfile = "release",
    *,
    repository_root: Path = REPOSITORY_ROOT,
    command_runner: CommandRunner = run_command,
    executable_locator: ExecutableLocator = shutil.which,
    progress: ProgressCallback = _no_progress,
) -> FirmwareArtifact:
    if profile not in ("debug", "release"):
        raise FirmwareBuildError(f"unsupported firmware build profile: {profile}")
    if not (repository_root / "CMakePresets.json").is_file():
        raise FirmwareBuildError(f"firmware repository was not found: {repository_root}")

    cmake = executable_locator("cmake")
    if cmake is None:
        raise FirmwareBuildError("CMake was not found; install it with `brew install cmake`")
    if executable_locator("ninja") is None:
        raise FirmwareBuildError("Ninja was not found; install it with `brew install ninja`")

    preset = f"firmware-{profile}"
    progress(ProgressEvent("build", f"Configuring {profile} firmware"))
    try:
        configured = command_runner(
            (cmake, "--preset", preset), cwd=repository_root, timeout_seconds=60
        )
        if configured.returncode != 0:
            raise FirmwareBuildError(
                "firmware configuration failed:\n" + failure_detail(configured)
            )
        progress(ProgressEvent("build", f"Building {profile} firmware"))
        built = command_runner(
            (cmake, "--build", "--preset", preset),
            cwd=repository_root,
            timeout_seconds=300,
        )
    except ExternalCommandError as error:
        raise FirmwareBuildError(str(error)) from error
    if built.returncode != 0:
        raise FirmwareBuildError("firmware build failed:\n" + failure_detail(built))

    build_directory = repository_root / "build" / preset / "firmware"
    elf_path = build_directory / "openflightcomputer-flight-firmware.elf"
    bootloader_elf_path = build_directory / "openflightcomputer-bootloader.elf"
    application_bin_path = build_directory / "openflightcomputer-flight-firmware.bin"
    if (
        not elf_path.is_file()
        or not bootloader_elf_path.is_file()
        or not application_bin_path.is_file()
    ):
        raise FirmwareBuildError(
            f"firmware build completed without producing the expected ELF: {elf_path}"
        )
    version, build_id = _read_generated_identity(
        build_directory / "generated" / "firmware_identity.c"
    )
    application = application_bin_path.read_bytes()
    metadata_path = build_directory / "openflightcomputer-application-metadata.bin"
    metadata_path.write_bytes(
        struct.pack(
            "<IIII", 0x4F464341, 1, len(application), zlib.crc32(application)
        )
    )
    artifact = FirmwareArtifact(
        profile=profile,
        elf_path=elf_path.resolve(),
        bootloader_elf_path=bootloader_elf_path.resolve(),
        application_bin_path=application_bin_path.resolve(),
        application_metadata_path=metadata_path.resolve(),
        firmware_version=version,
        build_id=build_id,
    )
    progress(ProgressEvent("build", f"Built {artifact.elf_path}"))
    return artifact
