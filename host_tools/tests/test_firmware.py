from pathlib import Path

from openflightcomputer.external_tools import CommandResult
from openflightcomputer.firmware import build_firmware


def test_build_uses_presets_and_returns_generated_identity(tmp_path: Path):
    (tmp_path / "CMakePresets.json").write_text("{}")
    commands = []

    def run(arguments, *, cwd, timeout_seconds):
        commands.append(tuple(arguments))
        if "--build" in arguments:
            output = tmp_path / "build/firmware-debug/firmware"
            (output / "generated").mkdir(parents=True)
            (output / "openflightcomputer-flight-firmware.elf").write_bytes(b"elf")
            (output / "generated/firmware_identity.c").write_text(
                'const char firmware_version[] = "0.1.0";\n'
                'const char firmware_build_id[] = "abc1234-dirty";\n'
            )
        return CommandResult(tuple(arguments), 0, "", "")

    artifact = build_firmware(
        "debug",
        repository_root=tmp_path,
        command_runner=run,
        executable_locator=lambda name: f"/tools/{name}",
    )
    assert commands == [
        ("/tools/cmake", "--preset", "firmware-debug"),
        ("/tools/cmake", "--build", "--preset", "firmware-debug"),
    ]
    assert artifact.firmware_version == "0.1.0"
    assert artifact.build_id == "abc1234-dirty"
