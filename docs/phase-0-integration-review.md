# Phase 0 integration review

Milestone 0.13 reviews the foundation as one system rather than adding a new
flight subsystem. Host-verifiable policies are tested here; physical evidence
remains explicitly separated in `docs/hardware-validation-checklist.md`.

## Reviewed decisions

- A request ID is required on every USB command and echoed on every response
  that came from a valid envelope. Malformed requests receive `request_id:null`.
- Protocol version negotiation is deferred until independently released host
  software must support deployed older firmware.
- USB remains a physically trusted development/bench interface for first
  flight. Cryptographic authentication is outside the current scope.
- One `usb-service` task remains the sole main-context transport owner. Separate
  logging and command tasks would both need to advance the same queues and
  completion state, creating unnecessary ordering and ownership complexity.
- `OK`, `WARNING`, and `DEGRADED` health may eventually permit a new arm;
  `UNKNOWN` and `CRITICAL` must not. This admission policy is distinct from
  runtime fault response and final actuator gating.
- A warning or ordinary fault never automatically leaves `ARMED`. A future SD
  failure, for example, must be classified non-critical so flight behavior is
  not interrupted merely because storage degraded.
- Critical faults retain the existing synchronous transition to terminal
  `FAULT`. Future subsystem reviews must reserve critical classification for a
  condition whose defined safe response justifies that transition; controlled
  failures such as receiver loss may instead belong in `FAILSAFE`.
- Exhausting all 16 active-fault slots is itself treated as a critical internal
  diagnostic failure. The dropped counter still exposes lost detail, but the
  lifecycle becomes `FAULT`, so ordinary operation cannot continue with
  `UNKNOWN` health.

## Startup and runtime policy matrix

| Situation | Fault result | Lifecycle | Health | Execution |
| --- | --- | --- | --- | --- |
| Successful startup | No active fault | `DISARMED` | `OK` | Scheduler runs |
| Non-critical USB startup failure | Active ordinary fault | `DISARMED` | `DEGRADED` | Core scheduler runs without USB service |
| Critical board/timebase/task/scheduler startup failure | Active critical fault | `FAULT` | `CRITICAL` | Existing fatal path halts |
| Ordinary runtime fault while armed | Active ordinary fault | Remains `ARMED` | `DEGRADED` | No automatic disarm |
| Critical runtime fault while armed | Active critical fault | `FAULT` | `CRITICAL` | Future final actuator gate must apply defined safe output |
| Fault registry capacity exhausted | Dropped count increments | `FAULT` | `CRITICAL` | Lost detail remains observable |

The portable integration suite executes these lifecycle/fault/health chains.
Board-result-to-boot-status mappings and the final halt calls remain validated
by compilation and source review because `main()` directly owns hardware
initialization.

## USB concurrency audit

The transport retains the tester-proven single-producer/single-consumer model:

| State | Writer | Reader/consumer | Synchronization |
| --- | --- | --- | --- |
| Raw receive bytes and head | USB receive callback | `usb-service` | Main pop uses a short interrupt-disabled snapshot |
| Raw receive tail | `usb-service` | USB receive callback | Main writes while interrupts are disabled |
| Receive-overflow marker | USB receive callback | `usb-service` | Read-and-clear inside the same critical section |
| Completed-line queue | `usb-service` framing | Command processor in the same callback | Main context only |
| Transmit queue and count | Command/log adapters through `usb-service` | `usb-service` | Main context only |
| Transmit-complete flag | USB callback | `usb-service` | Volatile flag read-and-clear in a critical section |
| ISR drop counter | USB receive callback | Statistics snapshot | Snapshot with interrupts disabled |

No parser, state transition, health evaluation, formatting, queue removal, or
transfer start occurs in interrupt context. Disconnect/deinitialization clears
the active/completed flags but retains queued entries, allowing retransmission
after reconfiguration rather than releasing data whose completion is unknown.

The review added compile-time checks for nonzero queues and for the raw ring's
16-bit index range, and replaced reset logic tied to a literal two-entry depth
with whole-array initialization. No additional scheduled task or lock was
required.

## Remaining Phase 0 limits

- The health response still emits details in stable fault-slot order. Critical
  counts and overall state remain visible if later detail is response-truncated;
  severity-prioritized detail can be reconsidered with host-tool evidence.
- Logging and responses deliberately share a lossy, bounded diagnostic
  transport. Responses receive admission priority but cannot preempt a transfer
  already accepted by USB.
- The cooperative scheduler cannot recover from a callback that does not
  return, and physical worst-case execution time remains unmeasured.
- Health means no known active reported condition, not completed aircraft
  self-test or flight readiness.
- Actuator arming admission, command timeout, range limiting, failsafe behavior,
  and the final motor-output gate must be specified before Phase 1 can produce
  DShot output.
