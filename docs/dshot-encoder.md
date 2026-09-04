# DShot packet encoder

Milestone 1.4 adds only the hardware-independent outbound DShot frame format.
It does not select a DShot rate, calculate timer duty values, configure GPIO or
DMA, submit motor commands, request telemetry in production, or drive an ESC.

## Frame format

The 16-bit frame is transmitted most-significant bit first:

```text
bits 15..5  11-bit value
bit  4      telemetry request
bits 3..0   checksum
```

The 12-bit payload is `(value << 1) | telemetry`. Its checksum is the low
nibble of `payload ^ (payload >> 4) ^ (payload >> 8)`, and the final frame is
`(payload << 4) | checksum`.

The value space is deliberately separated by the public API:

| Values | Meaning | API |
| --- | --- | --- |
| `0` | Motor stop | `dshot_encode_throttle()` |
| `1..47` | Special ESC commands | `dshot_encode_command()` |
| `48..2047` | Throttle values | `dshot_encode_throttle()` |

This prevents a corrupted or incorrectly converted throttle value in the
reserved range from silently becoming an ESC command. Invalid arguments and
category violations leave the caller's destination unchanged.

The command encoder only separates and frames command values. Command-specific
rules such as repetition counts, required motor-stop intervals, telemetry
requirements, and whether commands are allowed at all are future explicit
policy; Milestone 1.4 does not schedule or transmit them.

## Boundaries

The encoder consumes protocol integers and returns a `uint16_t`. It contains no
HAL, STM32, timer, DMA, GPIO, scheduler, motor-command, normalized-float,
allocation, or mutable-global dependency. The later application-owned motor
backend adapter will own conversion from a validated normalized
`motor_command_t` to stop or `48..2047`; that policy cannot be bypassed by this
module's throttle entry point.

Bidirectional DShot uses an inverted outbound checksum and requires input
capture behavior. It is deliberately not supported by this ordinary DShot
encoder. The telemetry argument here is only the standard one-bit request for
a separate ESC telemetry response.

## Verification

Host tests cover known frames, including documented value 1046 without
telemetry (`0x82C6`), stop, minimum command, and maximum throttle. They also
exhaustively encode every value from 0 through 2047 with telemetry clear and
set, compare against an independent iterative-nibble checksum implementation,
verify field recovery, reject every command value through the throttle API,
and prove invalid calls do not replace existing output.

There is no physical verification claim in this milestone: correct bits in a
host value do not prove timer timing, DMA order, voltage, channel routing, or
ESC acceptance.

The frame layout, value ranges, and checksum calculation were cross-checked
against the maintained [Betaflight DShot documentation](https://www.betaflight.com/docs/development/Dshot)
and its [`prepareDshotPacket()` implementation](https://github.com/betaflight/betaflight/blob/master/src/main/drivers/dshot.c).
