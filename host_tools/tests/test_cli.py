import pytest

from openflightcomputer.cli import build_parser


@pytest.mark.parametrize(
    "arguments",
    [
        ["firmware", "build"],
        ["firmware", "flash"],
        ["device", "status"],
        ["device", "arm"],
        ["device", "disarm"],
        ["device", "monitor"],
        ["motor", "run", "--motor", "1", "--throttle", "0.02", "--duration", "0.25"],
        ["smoke", "--no-flash"],
    ],
)
def test_documented_commands_parse(arguments):
    assert build_parser().parse_args(arguments).command == arguments[0]
