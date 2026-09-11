# USB newline-delimited JSON protocol

Milestone 0.11 adds a bounded command channel to the existing USB CDC device.
Every inbound request and outbound response or log event is one complete JSON
object followed by `\n`. The protocol is intended for development and bench
inspection; it is not an authenticated remote-control interface.

## Requests

A request must be exactly one object with two string members and one unsigned
32-bit request ID, plus the documented command-specific members. Member order
is irrelevant, but missing, duplicate, additional, negative, or noncanonical
numeric members are rejected:

```json
{"type":"command","command":"status","request_id":42}
```

The supported commands are:

| Command | Effect |
| --- | --- |
| `status` | Report lifecycle state and monotonic uptime |
| `health` | Report derived overall health, lifecycle state, severity counts, and bounded active-fault details |
| `receiver` | Inspect the latest raw and normalized receiver state plus link and transport diagnostics |
| `imu` | Inspect the latest mapped IMU sample and acquisition/scheduler diagnostics while not flying |
| `arm` | Apply health admission, then submit `ARM_REQUESTED` to the lifecycle state machine |
| `disarm` | Submit `DISARM_REQUESTED` to the lifecycle state machine |
| `motor_test` | Submit a leased single-motor command through the production safety gate |
| `config_read` | Return the complete active flight configuration |
| `config_write` | Validate, persist, and atomically apply one complete configuration while disarmed |
| `config_reset` | Erase the override and apply the compiled JSON defaults while disarmed |

`arm` does not itself request nonzero output. The application safety policy
first requires `OK`, `WARNING`, or `DEGRADED` health;
`UNKNOWN`/`CRITICAL` returns `health_rejected` without sending an arm event.
An accepted arm request first returns `pending:true` while the motor task sends
ten frames reasserting all configured ESC directions. The public lifecycle
remains `DISARMED` during this short internal preparation and becomes `ARMED`
only after successful completion. The state machine remains the sole
transition authority: arm is accepted only in `DISARMED`, while disarm is
accepted in `ARMED` or `FAILSAFE`. Disarm also cancels a pending arm. `FAULT`
remains terminal until reset.

The only accepted manual output request is:

```json
{"type":"command","request_id":48,"command":"motor_test","motor":2,"throttle":0.100000}
```

Firmware accepts logical motors 1 through 4 and a normalized throttle from
zero through `1.000000`. The decimal has at most six fractional digits and is
converted to integer millionths before any float is created. Each request sets
exactly one selected motor and submits zero for the other three. Every request
passes through
`motor_control_submit()`, so lifecycle, health, freshness, mapping, backend,
and fault behavior are identical to other future command producers. An
accepted command is a 100 ms lease: without a fresh accepted request, the
1 kHz motor-control task transmits stop frames and enters failsafe.

Configuration uses complete documents rather than per-field mutations. The
write envelope contains the complete schema-5 object from
`config/default-flight-configuration.json`; this shell command shows the exact
wire representation without duplicating that large document here:

```text
{"type":"command","request_id":51,"command":"config_read"}
jq -c '{type:"command",request_id:52,command:"config_write",configuration:.}' config/default-flight-configuration.json
{"type":"command","request_id":53,"command":"config_reset"}
```

Only uppercase `PROPS_IN`/`PROPS_OUT`, `NORMAL`/`REVERSED`,
`CONTROL_POINTS`, and `LINEAR` values are accepted. Decimal controls, factors,
limits, and curve points use at most six fractional digits. Each curve accepts
two through eight monotonic points and every complete command/response remains
bounded by the 4,096-byte transport line capacity.
The `control.rate_controller` object selects `PID`, defines its maximum IMU
sample gap, and carries five bounded decimal parameters for each axis.

Write and reset are rejected with `state_rejected` unless the lifecycle is
`DISARMED` with no arm pending. A storage failure returns
`configuration_storage_error`. See `docs/flight-configuration.md` for the
schema, persistence, task, mixer, and DShot application behavior.

## Responses

Examples, each followed by one newline:

```json
{"type":"response","request_id":42,"command":"status","ok":true,"state":"DISARMED","control_source":"NONE","uptime_us":123456,"firmware_version":"0.1.0","build_id":"5db525a"}
{"type":"response","request_id":43,"command":"health","ok":true,"health":"OK","state":"DISARMED","fault_data_complete":true,"active_fault_count":0,"warning_count":0,"fault_count":0,"critical_count":0,"dropped_fault_count":0,"faults":[],"reported_fault_count":0,"truncated":false}
{"type":"response","request_id":44,"command":"receiver","ok":true,"available":true,"sequence":7,"age_us":1250,"freshness":"FRESH","channels":[174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189],"normalized":{"roll":-0.500000,"pitch":0.250000,"yaw":0.000000,"throttle":1.000000,"arm":true},"failsafe":{"state":"LIVE","action":"LIVE","stage_two_latched":false,"recovery_ready":false},"link_statistics_present":true,"uplink_rssi_dbm":-42,"uplink_link_quality_percent":99,"uplink_snr_db":8,"uart_bytes":135014,"valid_frames":5000,"crc_errors":0,"framing_errors":0,"dma_overruns":0,"dma_bytes_dropped":0}
{"type":"response","request_id":45,"command":"imu","ok":true,"available":true,"sequence":8100,"age_us":250,"freshness":"FRESH","acceleration_raw":{"x":20,"y":-30,"z":-16384},"gyroscope_raw":{"x":2,"y":-3,"z":1},"gyroscope_corrected_raw":{"x":0,"y":0,"z":0},"calibration":{"state":"READY","progress_permille":1000,"samples":500,"restarts":0,"bias_raw":{"x":2,"y":-3,"z":1}},"attitude":{"source_sequence":8100,"roll_millidegrees":105,"pitch_millidegrees":70,"filtered_gyroscope_millidps":{"x":0,"y":0,"z":0}},"processing":{"processed":7600,"duplicates":0,"rejected":0,"continuity_resets":0},"service":{"reads":8101,"published":8100,"source_errors":1},"task":{"executions":8101,"last_execution_us":37,"maximum_execution_us":45,"overruns":0,"missed_releases":0},"high_rate":{"budget_us":180,"utilization_permille":180}}
{"type":"response","request_id":44,"command":"arm","ok":true,"pending":true,"state":"DISARMED"}
{"type":"response","request_id":45,"command":"arm","ok":false,"state":"BOOT","error":"transition_rejected"}
{"type":"response","request_id":46,"command":"arm","ok":false,"state":"DISARMED","error":"health_rejected"}
{"type":"response","request_id":47,"command":"arm","ok":false,"state":"DISARMED","error":"motor_not_ready"}
{"type":"response","request_id":48,"command":"motor_test","ok":true,"state":"ARMED","motor":2,"throttle":0.100000}
{"type":"response","request_id":49,"command":"motor_test","ok":false,"state":"ARMED","motor":0,"throttle":0.020000,"error":"motor_not_allowed"}
{"type":"response","request_id":51,"command":"config_read","ok":true,"state":"DISARMED","source":"DEFAULT","configuration":{"schema_version":5,"motors":{},"mixer":{},"control":{},"receiver_failsafe":{},"imu":{}}}
{"type":"error","request_id":null,"error":"invalid_request"}
{"type":"error","request_id":50,"error":"unsupported_command"}
```

The compact `config_read` line above abbreviates the five complete nested
configuration objects for readability. Actual firmware responses include
every required schema-5 field and can be written back unchanged.

The receiver response is produced only when the USB command is dispatched. It
copies the receiver service's already-published raw and normalized snapshots;
it neither reads UART/DMA data nor reruns CRSF parsing. `age_us` is computed
from the receiver packet's capture timestamp and the monotonic clock at the
time of the request. If no valid control frame has been published,
`available` is false and `sequence`, `age_us`, `channels`, and `normalized` are
`null`; failsafe and transport diagnostics remain available. Link values are
meaningful only when `link_statistics_present` is true.

The command is observational: it cannot change lifecycle state, renew motor
authority, or submit motor commands. Repeated inspection is host-driven, so
the 1 kHz receiver task does not continuously construct or publish a separate
USB-only snapshot.

The IMU response follows the same request-driven rule. It copies the existing
IMU service snapshot and never initiates SPI traffic or another task release.
Raw values already use the configured body-axis mapping. The firmware also
returns the current roll/pitch estimate and filtered gyro values as bounded
integer milli-units, plus processing counters; presentation conversion belongs
to the host. The command returns `state_rejected` in
`ARMED` or `FAILSAFE`. If no valid sample exists, the snapshot fields and task
object are `null`, while service and combined high-rate counters remain
available.

Milestone 0.12 derives `OK`, `WARNING`, `DEGRADED`, `UNKNOWN`, or `CRITICAL`
from the existing lifecycle and fault authorities. Each serialized active fault
contains its ID, catalogue severity/source, occurrence count, validity-aware
timestamps, and optional context. `fault_data_complete` identifies an internal
registry drop, while `truncated` identifies records omitted only from this
bounded response. See `docs/health-reporting.md` for the complete policy.

The firmware echoes the request ID on every response to a valid request
envelope, including unsupported commands. It uses JSON `null` only when the
envelope is malformed and no trustworthy ID was parsed. Log events are
unsolicited and therefore have no request ID. Protocol version negotiation is
deferred until independently released host software must communicate with
older deployed firmware.

The status response separates semantic `firmware_version` from the dirty-aware
Git `build_id`. A clean build uses a seven-character commit such as `5db525a`;
an image built with local changes uses `5db525a-dirty`. These fields identify
the exact image for diagnostics, but they are not a protocol-version or
compatibility guarantee.

`control_source` is `NONE`, `USB_TEST`, or `RECEIVER`. A successful USB arm
selects `USB_TEST` after direction preparation completes, so only USB
motor-test commands can renew output until disarm. Commands from another
source cannot replace the retained command. Disarm and all fail-safe stop
paths remain source-independent.

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

The manufacturing acceptance run independently exposed both parts of this
contract as important: output state must not advance without retained storage
when transport admission is busy, and target newlib-nano cannot be assumed to
format `%llu` correctly. Response retention is already implemented here. The
shared bounded `uint64_decimal_format()` now serializes uptime, timestamps, and
sequences without depending on target long-long printf support.

## Reused tester implementation

The newline-framer algorithm, JSMN parser, raw receive ring, completed-line
queue pattern, and short receive callback were closely adapted from the proven
manufacturing tester. Flight firmware defines a smaller command envelope,
independent state-machine dispatch, its own response schemas, and no tester
session/component acceptance behavior.

## Physical verification boundary

Host tests cover fragmentation, CRLF, exact-size and oversized lines, recovery,
strict parsing, response bytes, transition acceptance/rejection, pending
response retry, and JSON log escaping. Flight Computer V1 has physically
enumerated with the V1 VBUS workaround, and the flashed flight image has served
live RP1 receiver inspection to the host view. Extended disconnect/reconnect
and overflow stress under a real host remain pre-flight validation work.

## Propeller-free host workflow

The reusable host workflow behind `./ofc motor run` requires the board to
already report `ARMED`; it never arms implicitly. It sends zero-throttle
commands for five seconds so the ESC can recognize the DShot stream, refreshes
the selected throttle every 20 ms for the requested positive finite duration,
then sends five explicit zero requests and disarms. The same cleanup is
attempted after Ctrl-C or a command error. Firmware remains authoritative if the process, USB
connection, or host computer disappears because the 100 ms lease expires
independently.

An accepted `motor_test` request updates one complete retained command and its
producer timestamp. It does not directly emit one physical frame. The existing
highest-priority 1 kHz motor task rechecks lifecycle, health, lease freshness,
and prior DMA completion before emitting each one-shot DShot frame. Thus USB
refresh rate renews authority while the onboard task owns waveform cadence.

The public protocol deliberately omits raw DMA register snapshots and retained
frame dumps. Backend failures are represented by compact numeric context in
the existing fault system, while detailed register inspection remains a
debugger activity rather than a permanent USB API.
