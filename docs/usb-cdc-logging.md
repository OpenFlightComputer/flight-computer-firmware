# USB CDC logging backend

Milestone 0.10 connected the fixed logging queue to the Flight Computer V1
USB-C data connection. Milestone 0.11 retains that non-blocking producer path,
serializes logs as newline-delimited JSON, and shares the transport with
bounded command responses.

## Proven implementation boundary

The hardware and STM32 USB behavior is adapted closely from the manufacturing
tester. Both implementations use:

- PA11/PA12 as OTG FS DM/DP on AF10 and PA9 for VBUS sensing;
- the 48 MHz PLLQ clock already established by board initialization;
- the embedded Full-Speed PHY, four endpoints, no USB DMA, and VBUS sensing;
- RX FIFO `0x80` words and TX FIFOs `0x40`, `0x60`, and `0x20` words;
- the official STM32CubeF4 USB Device CDC class;
- static class storage in place of the library's allocation hooks;
- OTG FS interrupt priority 6; and
- interrupt callbacks that only advance USB state, copy packet data,
  or mark a transmission complete.

Milestone 0.11 also closely adapts its fixed raw receive ring, newline framer,
completed-line queue, and JSMN parser. The flight firmware does not copy the
tester application loop, session/component protocol, registry, or acceptance
policy.

## Layer ownership

| Module | Responsibility |
| --- | --- |
| `hardware/boards/flightcomputer_v1/usb_device_port.c` | Proven V1 pins, OTG FS peripheral setup, FIFO layout, and STM USB low-level callbacks |
| `peripherals/usb/usb_descriptors.c` | CDC identity and descriptor provider |
| `peripherals/usb/usb_cdc_transport.c` | Fixed receive/transmit queues and CDC transfer state |
| `peripherals/usb/newline_framer.c` | Bounded newline and CRLF framing with overflow recovery |
| `peripherals/usb/usb_json_protocol.c` | Strict request parsing and bounded response serialization |
| `app/usb_command_processor.c` | Command dispatch, state-machine events, and response retry |
| `app/usb_logging_backend.c` | JSON log serialization and mapping transport results to the logging contract |
| `app/main.c` | Initialization policy, fault reporting, backend attachment, and scheduler registration |

The application layer sees no STM32 type or HAL call. The transport header is
also free of STM32 types.

## Runtime flow and ownership

```text
producer
   ↓ bounded formatting
32-record logging queue
   ↓ one logging_drain_once() attempt
USB logging backend
   ↓ JSON line bounded to 768 bytes
two-entry USB-owned transmit queue
   ↓ asynchronous CDC transfer
host serial device
```

The backend formats the oldest immutable `log_record_t` into a stack buffer.
`usb_cdc_transport_try_write()` copies the complete line into a transport-owned
entry before returning `USB_CDC_WRITE_ACCEPTED`. Only then does the logging
core remove the original record. A busy transport retains the original record
for a later release. An invalid/error result removes and counts that record so
one bad item cannot permanently block the queue.

Each transport entry holds exactly one complete formatted line, up to 768
bytes, including its newline. There are two entries. The USB library may split
that line into multiple Full-Speed packets, but the entry remains owned until
the transmit-complete callback marks the entire CDC request complete.

## Scheduled service

The shared `usb-service` task has:

```text
period:   1,000 us (1,000 Hz eligibility)
priority: TASK_PRIORITY_BACKGROUND (255, lowest)
work:     process up to 64 RX bytes and USB TX state; process/retry one
          command response; then attempt one log drain
```

The task is registered only after USB initialization, backend attachment, and
command-processor initialization succeed. Ready-batch scheduler behavior means
it cannot displace a higher-priority task ready in the same batch. Its callback
contains no wait loop. Command responses are attempted before logs.

The 1 ms period gives prompt interactive output and a maximum admission rate
of one record per millisecond while keeping each invocation bounded. It is not
a promise that a host will physically consume 1,000 lines per second.

## Disconnect, congestion, and failure policy

USB is diagnostic, not a flight-safety authority:

- no host connection is an ordinary state, not a fault;
- the transport can accept two records while disconnected, then reports busy;
- remaining records stay in the 32-entry logging queue;
- if both queues fill, the existing logging policy drops and counts new logs;
- reconnecting allows queued output to resume without resetting firmware;
- runtime transfer-start failures are counted and retried without blocking;
- USB initialization/backend failure reports the non-critical
  `FAULT_ID_USB_LOGGING_INITIALIZATION`; command-processor initialization
  failure reports `FAULT_ID_USB_COMMAND_INITIALIZATION`. Either leaves the
  service task detached and permits startup to continue into `DISARMED`.

Because the backend is asynchronous, a fatal path that immediately halts the
MCU still cannot guarantee that its final queued log reaches the host. The
fault registry and boot status remain the authoritative debugger-visible
diagnostics. A synchronous panic transport remains outside this milestone.

## Receive boundary

The CDC receive interrupt copies into a 512-byte ring and immediately re-arms
the OUT endpoint. Main context processes at most 64 bytes per service release,
frames lines up to 256 bytes, and retains two completed lines. Raw-byte,
completed-line, and oversized-line drops are counted. Any raw overflow forces
discard through the next newline, preventing a partial request from executing.
See `docs/usb-json-protocol.md` for the request and response contract.

## USB identity

The default identity is development-only `0xCAFE:0x4002`, with product string
`OpenFlightComputer Flight Firmware`. It is deliberately distinct from the
manufacturing tester's development PID `0x4001`. CMake refuses to present the
default identity as distribution-approved. Assigned VID/PID values are still
required before hardware distribution.

## Physical verification boundary

Host tests validate exact JSON backend formatting/escaping,
accepted/busy/error mapping, retry behavior, newline framing, parsing, and
command dispatch. Debug and Release builds validate complete STM32 USB linkage
and the interrupt vector. Enumeration, VBUS behavior, CDC transmission and
receive, disconnect/reconnect, and sustained physical throughput still require
a connected Flight Computer V1 and host.
