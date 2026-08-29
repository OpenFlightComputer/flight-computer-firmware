# Fault system

Milestone 0.8 adds a deterministic, allocation-free fault registry and connects
critical fault policy to the application state machine. The registry preserves
diagnostic information; it does not log, print, or access peripherals.

## Policy catalogue

Detection and classification are separate responsibilities. A producer reports
only a fault ID and optional context. A central immutable catalogue maps that
ID to severity and source, so individual report sites cannot silently choose a
different safety response.

The production catalogue currently contains only failures that the firmware can
already detect:

| Fault ID | Severity | Source |
| --- | --- | --- |
| MCU initialization | Critical | MCU |
| Clock configuration | Critical | MCU |
| Unexpected clock frequency | Critical | MCU |
| Timebase configuration | Critical | MCU |
| Task registration | Critical | Application |
| Scheduler initialization | Critical | Scheduler |
| Scheduler invalid runtime state | Critical | Scheduler |
| State-machine transition | Critical | State machine |
| Fault-clock attachment | Critical | Application |
| Logging-clock attachment | Fault | Application |
| USB logging initialization | Fault | USB |
| USB command initialization | Fault | USB |

Warning and non-critical fault behavior is host-tested with a test catalogue.
The first production non-critical ID records failure to attach the logging
clock: diagnostics remain operational without valid timestamps, so lifecycle
state does not change. USB logging or command initialization is also
non-critical because USB is a development/diagnostic channel: its service task
remains detached while firmware continues without host access. Other
production IDs will be added only when an owning
subsystem defines a real detectable condition and its safety consequence.

## Severity behavior

| Severity | Lifecycle effect | Clear behavior |
| --- | --- | --- |
| Warning | Record only; normal operation may continue | Clearable |
| Fault | Record degraded/disabled capability; lifecycle state is unchanged | Clearable |
| Critical | Record, then synchronously request `SYSTEM_STATE_FAULT` | Latched until reset |

The fault record is created or updated before a critical transition is
requested. Diagnostic evidence therefore remains available even if the state
transition itself fails. If the system is already in `SYSTEM_STATE_FAULT`, a
new or repeated critical fault updates diagnostics without submitting a
redundant state event.

The state machine accepts `FAULT_DETECTED` from every non-fault state. A
critical startup report therefore moves `INITIALIZING` to terminal `FAULT`, and
a later arm request is rejected. The fault engine changes lifecycle state but
does not halt the MCU. Existing fatal application paths still halt explicitly
after reporting; future runtime policy may remain alive for diagnostics while
the final actuator gate enforces non-`ARMED` output inhibition.

## Records and repeated occurrences

The registry has 16 active slots. Each record contains:

- fault ID;
- catalogue-owned severity and source;
- first and most recent timestamps plus separate validity flags;
- most recent optional 32-bit context plus a validity flag;
- saturating occurrence count;
- active state.

Fault IDs are unique within a catalogue. Reporting an active ID updates its
existing slot, most recent timestamp, context, and occurrence count. It does not
consume another slot. Clearing a warning or ordinary fault releases its slot;
critical records cannot be cleared.

If all slots are occupied, a new record is dropped and the saturating dropped
counter increments. A new critical incident still requests
`SYSTEM_STATE_FAULT` even when no record slot is available. Capacity pressure
can therefore reduce diagnostics but cannot suppress the safety transition.

The catalogue itself is also bounded to 32 definitions, rejects invalid IDs,
severities, sources, and duplicates during initialization, and must remain
alive for the lifetime of the fault system. Production uses a static catalogue.

## Time during startup

The fault system starts before board initialization, when the monotonic TIM5
clock may not exist. Reports made at that stage store zero with timestamp-valid
flags cleared. This explicitly represents unavailable timing rather than
reading an unconfigured timer or presenting zero as a real time.

After successful board initialization, the application attaches `time_us()`.
Subsequent reports receive valid microsecond timestamps. If an early fault is
reported again later, its first timestamp remains marked unavailable while its
latest timestamp becomes valid.

## Context and diagnostics

Context is an optional raw `uint32_t` owned by each fault ID's documentation.
Current application reports use it for the originating board, task,
scheduler, or state-event result. The fault system does not interpret context.
Future structured health and logging milestones may format known IDs and their
context without changing the realtime reporting API.

`boot_status_t` remains separate. It retains the compact reason a fatal startup
path halted, while the fault registry adds source, severity, timing, repeated
occurrence, and context information.

## Concurrency boundary

Reporting, clearing, clock attachment, and state transitions currently belong
to main context. The API is not safe for concurrent interrupt mutation. Future
interrupt-driven modules must publish bounded events for main-context fault
reporting unless a later milestone introduces and validates an explicit
critical-section policy.
