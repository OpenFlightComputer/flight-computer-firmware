import pytest

from openflightcomputer.cli import build_parser


@pytest.mark.parametrize(
    "arguments",
    [
        ["firmware", "build"],
        ["firmware", "flash"],
        ["device", "status"],
        ["device", "monitor"],
        ["smoke", "--no-flash"],
    ],
)
def test_documented_commands_parse(arguments):
    assert build_parser().parse_args(arguments).command == arguments[0]
