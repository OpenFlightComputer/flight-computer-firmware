# Logging core

Milestone 0.9 provides a central hardware-independent logging facade. Producers
capture timestamped records into a fixed RAM queue and return without touching
an output device. Milestone 0.10 attaches the USB CDC backend through scheduled
bounded drain attempts without changing producer behavior.

## Levels

| Level | Standard |
| --- | --- |
| `DEBUG` | Detailed developer values and decision paths; disabled by the default threshold |
| `INFO` | Expected lifecycle events and meaningful successful operations |
| `WARN` | Unexpected but recoverable condition; normal operation can continue |
| `ERROR` | An operation or subsystem failed, but firmware can remain operational |
| `FATAL` | Diagnostic message associated with an unrecoverable condition |

`LOG_THRESHOLD_OFF` is a configuration value, not a record level. `TRACE` is
deferred until evidence shows that `DEBUG` is insufficient.

Log level does not determine system safety or transport behavior. In
particular, `LOG_FATAL()` neither reports a critical fault nor transitions the
state machine. The structured fault system remains the safety authority and
retains critical diagnostics independently of the lossy logging queue.

## Modules and configuration

The currently defined modules are `SYSTEM`, `BOARD`, `TIMEBASE`, `TASK`,
`SCHEDULER`, `STATE`, `FAULT`, and `USB`. Module enums prevent inconsistent free-form
names. Future milestones add a module only when its owning subsystem exists.

The global threshold defaults to `INFO`. A record passes when its level is at
least the effective threshold. Each module can override the global threshold
with `DEBUG` through `FATAL`, or `OFF`; clearing the override restores global
inheritance. Configuration changes are synchronous main-context operations.

Names and the default threshold live in `logging_config.c`. Level/module
semantics, formatting, ownership, and realtime rules live in this document.
The output destination is selected by attaching a backend, never at a producer
call site.

## Producer API

Normal usage is:

```c
LOG_INFO(LOG_MODULE_SYSTEM, "initialization complete");
LOG_DEBUG(LOG_MODULE_SCHEDULER,
          "task=%u duration_us=%lu",
          task_index,
          execution_time_us);
```

The macros check filtering before their format arguments are evaluated. An
enabled call then:

1. assigns a 64-bit sequence number;
2. rejects immediately if the queue is full;
3. captures the current clock and validity at the start of `logging_write()`;
4. formats into the fixed record message;
5. publishes the complete record to the queue.

This captures event order and time immediately in RAM but defers all backend
output. Two records with the same microsecond timestamp remain ordered by their
sequence numbers. There is deliberately no synchronous `LOG_IMMEDIATE` path.

Formatting uses bounded `vsnprintf()` with integer, hexadecimal, character,
and string arguments. Format strings must be static firmware-owned strings;
external data must never become the format string. Every `%s` argument must be
known null-terminated and bounded, or use an explicit precision, because the C
formatter may scan the full input even when its output buffer is fixed.
Floating-point formatting is excluded from the initial standard because
embedded nano-printf does not include it by default and its cost is
inappropriate without a measured requirement. Expensive formatting can still
perturb hot code, so high-rate paths should prefer existing timing statistics,
counters, or a later evidence-driven binary trace facility.

## Record and queue

The queue contains 32 records. Each record has 95 usable message characters
plus its terminator, timestamp and validity, 64-bit sequence, level, module,
message length, and truncation flag. The complete queue consumes approximately
4 KiB of RAM.

Messages longer than 95 characters are null-terminated, enqueued with
`truncated` set, and counted. A formatting failure is counted and does not
publish a partial record.

When all 32 slots are occupied, the new record is dropped, its sequence number
is consumed, and total/per-level saturating drop counters increment. Existing
records remain in FIFO order. The producer never waits, scans the queue, evicts
another level, or calls a backend. A later sequence gap makes a dropped attempt
observable after draining resumes.

## Time before board initialization

The logger initializes before the board and captures the boot message with an
invalid timestamp. After successful board initialization, the application
attaches `time_us()`. Later records contain valid monotonic microsecond times.
This avoids reading an unconfigured timer or presenting zero as a real event
time.

If clock attachment fails, logging remains usable with invalid timestamps and
the application reports the non-critical
`FAULT_ID_LOGGING_CLOCK_ATTACHMENT`. This degrades diagnostics without entering
`SYSTEM_STATE_FAULT`.

## Backend and drain contract

A backend receives a pointer to the oldest immutable structured record for the
duration of one callback and returns immediately:

| Result | Queue behavior |
| --- | --- |
| `LOG_BACKEND_ACCEPTED` | Backend copied/accepted the record; remove it |
| `LOG_BACKEND_BUSY` | Retain the record and retry later |
| `LOG_BACKEND_ERROR` | Count the error and remove the record to avoid head-of-line blocking |

`logging_drain_once()` processes at most one record. With no backend, it leaves
the queue unchanged.

Milestone 0.10 attaches the USB CDC backend and registers a 1,000 us background
task. That task first advances USB completion/start state, then makes at most
one drain attempt per release. Accepted lines have already been copied into
one of two USB-owned 160-byte entries. USB interrupt handling only advances the
device/transfer state; it does not format or drain application records. See
`docs/usb-cdc-logging.md` for the transport and disconnect policy.

## Canonical text format

The bounded formatter produces:

```text
[0000008121] #0000000042 INFO  STATE     INITIALIZING -> DISARMED
```

Before the clock is available:

```text
[----------] #0000000001 INFO  SYSTEM    OpenFlightComputer booting
```

The formatter owns timestamp, sequence, level/module names, spacing, message,
and newline. A backend owns only transport. An asynchronous backend must copy
or format a record into storage whose lifetime covers its transfer before it
returns `LOG_BACKEND_ACCEPTED`.

## Concurrency and debugging boundary

Logging configuration, production, and draining belong to main/cooperative-task
context. The queue has no locks and is not interrupt-safe. Interrupt handlers
must update a bounded counter/timestamp or publish a small module-owned event
for later logging in main context.

The startup integration logs boot, board and USB initialization, task registration,
scheduler initialization, the transition to `DISARMED`, normal running, and
fatal stop information. The 1,000 Hz diagnostic task intentionally emits no
logs because doing so would manufacture overload instead of useful evidence.

An immediate panic/crash transport is outside Milestone 0.9. If later evidence
shows that queued diagnostics are lost during reset or hard fault, that should
be a separate fixed and strictly bounded panic-diagnostics design rather than
a severity level.
