# Actuator safety gate

Milestone 1.7 establishes the sole production boundary through which a motor
command may eventually reach physical output. It is hardware-independent and
does not configure GPIO, TIM8, DMA, DShot transmission, or a scheduled output
task.

## Ownership and call path

```text
USB / future receiver / future flight controller
                       |
                       v
          motor_control_submit()
                       |
        state + health + validity + freshness
                       |
        private logical-to-physical mapping
                       |
        private motor_output_t singleton
                       |
          future private DShot backend
```

`motor_control.c` owns the only production `motor_output_t`, current motor
mapping, last accepted command, clock and timeout. None has a public getter.
The public API accepts logical commands, requests an unconditional stop,
periodically synchronizes safety state, and configures mapping only under its
existing disarmed/stopped rule.

Initialization is deliberately in `motor_control_internal.h`. A later
application-composition milestone may provide the one real backend descriptor,
but command producers must never receive that descriptor or the private output
instance.

An automated source-boundary check runs in every normal host and firmware build
and as a named CTest. It rejects production uses of the raw `motor_output`
submission API, DShot encoder/timing entry points, or new arm-event sites
outside their explicit owner files. This is an accidental-bypass guard, not a
security boundary against someone deliberately changing the check and
rebuilding the firmware.

## Submission decision

A command passes only when all of these are true:

1. motor control initialized successfully and its initial force-stop was
   accepted;
2. lifecycle state is exactly `ARMED`;
3. health is `OK`, `WARNING`, or `DEGRADED`;
4. the complete four-motor command is structurally valid; and
5. its timestamp is not in the future and is within the configured inclusive
   freshness timeout.

The command is recreated through `motor_command_create()` before use, then
mapped as one complete snapshot. This repeats finite/range/near-zero checks at
the final boundary even if a caller manually corrupted a public structure.

`WARNING` and `DEGRADED` deliberately remain flyable. An ordinary storage or
logging degradation appearing after arming does not change lifecycle state or
stop output. `UNKNOWN` is treated as lost safety knowledge: it blocks output,
enters `FAILSAFE` when currently armed, and forces stop. `CRITICAL` normally
already means the fault system synchronously entered terminal `FAULT`; the gate
also refuses and stops independently.

An invalid or stale command while armed enters `FAILSAFE` and forces stop. A
fresh command cannot resume output from `FAILSAFE`; the existing lifecycle
requires an explicit disarm before another arm request.

## Periodic synchronization

`motor_control_synchronize()` must be called on every future motor-output
service release, even if no producer supplied a new command. It rechecks:

- lifecycle state;
- current health; and
- freshness of the last command actually accepted by the backend.

This prevents an ESC from retaining an old throttle merely because command
production stopped. A backend `BUSY` result does not refresh the retained
timestamp because that new command was not accepted. The previous accepted
command therefore still expires normally.

The current default command timeout is 100,000 microseconds. The timer-output
milestone must choose and document a service frequency; that period determines
the small additional detection latency beyond the timeout. State transitions
must also be followed by synchronization in the same main-context service
cycle. No interrupt calls this API.

## Force-stop and fault policy

Initialization begins with the generic motor-output facade's independently
accepted initial stop. Initialization also verifies that every motor and
state-transition fault ID it depends on exists in the injected catalogue with
critical severity. Normal rejection avoids repeatedly calling the backend when
the private state already records an accepted stop.

Once output might be active, the following conditions override normal demand:

| Condition | Response |
| --- | --- |
| Any lifecycle state other than `ARMED` | Force stop |
| `UNKNOWN` or `CRITICAL` health | Enter failsafe when applicable; force stop |
| Invalid or stale command | Enter failsafe; force stop |
| Mapping invariant failure | Critical motor-output fault; force stop |
| Backend submission error | Critical motor-output fault; force stop |
| Force-stop rejection/error | Critical force-stop fault; physical state remains explicitly unknown |

The catalogue owns three new critical IDs: motor initialization, motor output,
and motor force-stop. Critical reporting remains the existing fault
system's responsibility and synchronously drives lifecycle `FAULT`.

`force_stop` has no busy result. An accepted stop invalidates the retained
command. A failed stop never sets the private `outputs_stopped` flag, so mapping
changes and any claim of a safe stopped output remain prohibited.

## Arm admission

The shared application safety policy permits USB arming only for `OK`,
`WARNING`, or `DEGRADED` health. `UNKNOWN` and `CRITICAL` return a distinct
`health_rejected` response before an arm event reaches the state machine.
The state machine still independently requires the current lifecycle state to
allow an arm request.

This currently preserves the development-only ability to enter logical
`ARMED` before a physical backend exists. It cannot produce motor output:
motor submission returns `NOT_INITIALIZED`, and no production backend is
attached. When the backend is integrated, its successful initialization and
initial stop become mandatory before the application exposes manual motor
commands.

## Verification boundary

Host tests use an injected copying backend and cover initialization failure,
initial-stop failure, private mapping, accepted/busy/error submissions,
warning/degraded continuation, state and health blocking, invalid and stale
commands, timeout synchronization, failsafe entry, critical fault reporting,
force-stop failure, and stopped-state tracking. Separate policy tests cover all
health values, while USB tests cover health-rejected arm requests.

These tests prove software decisions and callback ordering only. They do not
prove GPIO state, DMA cancellation, timer preload behavior, waveform timing,
ESC recognition, motor response, or physical stop. Those remain staged,
propeller-free hardware milestones.
