# System state and safety boundary

Milestone 0.7 introduces the central application lifecycle state. The state
machine is hardware-independent, allocation-free, synchronous, and driven by
explicit events. It does not control motors or infer state from peripherals.

## States

| State | Meaning |
| --- | --- |
| `BOOT` | Software state immediately after state-machine initialization |
| `INITIALIZING` | Board, task registry, and scheduler initialization is in progress |
| `DISARMED` | Initialization succeeded; future actuator output must be inhibited |
| `ARMED` | An explicit arm request was accepted; future actuator output may be considered |
| `FAILSAFE` | A failsafe was detected while armed; future actuator policy must inhibit or explicitly handle output |
| `FAULT` | A fatal condition occurred; the state is terminal until reset |

The application reaches `DISARMED` after successful startup and never requests
`ARMED` by itself. Milestone 0.11 adds an explicit USB `arm` command as a
development/bench lifecycle-event source. It has no actuator effect because no
motor subsystem exists. Before a later actuator command can be considered safe,
its authorization and final output-gating policy must be designed and tested.

## Legal transitions

| Current state | Event | Next state |
| --- | --- | --- |
| `BOOT` | initialization started | `INITIALIZING` |
| `INITIALIZING` | initialization completed | `DISARMED` |
| `DISARMED` | arm requested | `ARMED` |
| `ARMED` | disarm requested | `DISARMED` |
| `ARMED` | failsafe detected | `FAILSAFE` |
| `FAILSAFE` | disarm requested | `DISARMED` |
| Any state except `FAULT` | fault detected | `FAULT` |

Every other state/event pair is rejected without changing either the current
or previous state. In particular:

- initialization cannot directly produce `ARMED`;
- `FAILSAFE` cannot return directly to `ARMED`;
- `FAULT` rejects every event and requires an MCU reset;
- repeated arm, disarm, initialization, failsafe, and fault events are not
  silently treated as successful transitions.

## Disarm authority

The state transition API is synchronous. An accepted disarm request changes
`ARMED` or `FAILSAFE` to `DISARMED` before returning. Once an actuator subsystem
exists, its final output gate must consume this authoritative state so that
any state other than `ARMED` prevents normal motor demand from reaching the
hardware.

Milestone 0.7 establishes that software contract but does not implement motor
outputs. The DShot milestone must enforce the gate at the final actuator
boundary; merely checking state at an earlier command source would not be
sufficient.

## State versus boot status and faults

`system_state_machine_t` is the lifecycle authority. It records current state,
previous state, accepted-transition count, and rejected-transition count.
Counters saturate instead of wrapping.

`boot_status_t` remains a separate detailed startup/debug diagnostic. For
example, a clock configuration failure sets system state to `FAULT` while its
boot status identifies the specific clock error. Keeping these concepts
separate prevents hardware initialization details from becoming lifecycle
states.

Milestone 0.8 adds fault identifiers, catalogue-owned severity and source,
timestamps, occurrence/context metadata, and the policy that converts critical
fault records into the existing `FAULT_DETECTED` event. Warning and ordinary
fault records do not change lifecycle state. Critical records are latched, and
their diagnostic record is preserved before the synchronous state transition.
See `docs/fault-system.md` for the complete policy and capacity behavior.

## Concurrency boundary

The application and USB command processor call the state machine only from main
context. It does not
provide interrupt-safe concurrent mutation. Future interrupt handlers must
publish bounded events for main-context handling rather than mutate system
state directly, unless a later design explicitly adds and validates a critical
section policy.

Milestone 0.12 health labels are a read-only diagnostic projection and do not
alter this authority. In particular, `WARNING`, `DEGRADED`, and `UNKNOWN` do not
implicitly accept or reject arm requests. Any such relationship must be added
later as an explicit state/fault safety policy and enforced at the final
actuator boundary.
