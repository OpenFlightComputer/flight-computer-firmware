# USB newline-delimited JSON protocol

Milestone 0.11 adds a bounded command channel to the existing USB CDC device.
Every inbound request and outbound response or log event is one complete JSON
object followed by `\n`. The protocol is intended for development and bench
inspection; it is not an authenticated remote-control interface.

## Requests

A request must be exactly one object with two string members. Member order is
irrelevant, but missing, duplicate, or additional members are rejected:

```json
{"type":"command","command":"status"}
```

The supported commands are:

| Command | Effect |
| --- | --- |
| `status` | Report lifecycle state and monotonic uptime |
| `health` | Report lifecycle state plus active and dropped fault counts |
| `arm` | Submit `ARM_REQUESTED` to the lifecycle state machine |
| `disarm` | Submit `DISARM_REQUESTED` to the lifecycle state machine |

`arm` changes only the software lifecycle state. This firmware contains no
motor driver or actuator output, so accepting it cannot drive hardware. The
state machine remains the sole transition authority: for example, arm is
accepted only in `DISARMED`, while disarm is accepted in `ARMED` or `FAILSAFE`.
`FAULT` remains terminal until reset.

## Responses

Examples, each followed by one newline:

```json
{"type":"response","command":"status","ok":true,"state":"DISARMED","uptime_us":123456}
{"type":"response","command":"health","ok":true,"state":"DISARMED","active_fault_count":0,"dropped_fault_count":0}
{"type":"response","command":"arm","ok":true,"state":"ARMED"}
{"type":"response","command":"arm","ok":false,"state":"BOOT","error":"transition_rejected"}
{"type":"error","error":"invalid_request"}
{"type":"error","error":"unsupported_command"}
```

The 0.11 `health` response is deliberately a small foundation summary.
Per-fault structured detail and richer subsystem health belong to Milestone
0.12.

## Log events

USB logs use the same newline-delimited JSON stream:

```json
{"type":"log","timestamp_us":42,"sequence":1,"level":"INFO","module":"STATE","message":"DISARMED -> ARMED source=usb","truncated":false}
```

Before the timebase is available, `timestamp_us` is `null`. Quotes,
backslashes, controls, and non-ASCII bytes in messages are escaped, so one log
record cannot break framing or inject another JSON object. The logging core's
canonical text formatter remains available for non-USB consumers; the USB
backend serializes the structured record directly as JSON.

## Bounds, scheduling, and backpressure

The receive interrupt only copies bytes into a 512-byte single-producer,
single-consumer ring and immediately re-arms USB. The 1 ms background
`usb-service` task performs all framing, parsing, state changes, response
building, and logging work in main context. Each release:

1. processes at most 64 received bytes and advances transmit state;
2. retries one pending response or handles at most one completed command; and
3. attempts to drain at most one log record.

Input lines may contain at most 256 bytes before the newline. An oversized or
raw-ring-overflowed line is discarded through its next newline so a truncated
fragment is never interpreted as a command. Two completed input lines can
wait for dispatch. All overflow/drop counters saturate.

Responses have priority over log admission. If the two-entry transmit queue is
busy, one complete response remains in the command processor's fixed 768-byte
buffer and no next command is consumed until that response is accepted or the
transport reports an error. This preserves response order without waiting or
allocating memory. The same 768-byte transmit-entry bound holds a worst-case
escaped 95-character log message.

## Reused tester implementation

The newline-framer algorithm, JSMN parser, raw receive ring, completed-line
queue pattern, and short receive callback were closely adapted from the proven
manufacturing tester. Flight firmware defines a smaller command envelope,
independent state-machine dispatch, its own response schemas, and no tester
session/component acceptance behavior.

## Physical verification boundary

Host tests cover fragmentation, CRLF, exact-size and oversized lines, recovery,
strict parsing, response bytes, transition acceptance/rejection, pending
response retry, and JSON log escaping. Physical enumeration, packet delivery,
disconnect/reconnect, overflow under a real host, and interactive commands
still require a connected Flight Computer V1.
