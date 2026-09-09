# Motor safety gate

Milestone 1.7 establishes the sole production boundary through which a motor
command may reach physical output. Milestones 1.8 and 1.9 attach the private
four-channel DShot300 backend and a highest-priority 1 kHz synchronization
task without exposing raw hardware access to command producers.

## Ownership and call path

```text
USB / future receiver / future flight controller
                       |
                       v
       motor_control_submit(source, command)
                       |
 source + state + health + validity + freshness
                       |
        private logical-to-physical mapping + retained command
                       |
                       v
       1 kHz motor_control_synchronize()
                       |
     repeat while state/health/lease remain valid
                       |
        private motor_output_t singleton
                       |
          private DShot300 backend
                       |
                       v
        TIM8 update DMA to all four outputs
```

`motor_control.c` owns the only production `motor_output_t`, current motor
mapping, retained command, clock and timeout. None has a public getter.
The public API atomically arms with a named source, accepts source-tagged
logical commands, requests source-independent disarm or emergency stop,
periodically synchronizes safety state, and configures mapping only while
disarmed. Mapping remains safe while stop frames stream because all four
physical throttle values are zero.

Initialization is deliberately in `motor_control_internal.h`. Application
composition supplies the real backend descriptor, but command producers never
receive that descriptor or the private output instance.

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
2. the command source is the source that successfully armed motor control;
3. lifecycle state is exactly `ARMED`;
4. five seconds of periodic DShot stop frames have completed since startup or
   the most recent emergency hardware stop;
5. health is `OK`, `WARNING`, or `DEGRADED`;
6. the complete four-motor command is structurally valid; and
7. its timestamp is not in the future and is within the configured inclusive
   freshness timeout.

A wrong-source command is rejected without modifying the retained command.
Disarm, failsafe, faults, and force-stop are never blocked by source ownership.
See `control-authority.md` for the complete handoff contract.

The command is recreated through `motor_command_create()` before use, then
mapped as one complete snapshot. This repeats finite/range/near-zero checks at
the final boundary even if a caller manually corrupted a public structure.

`WARNING` and `DEGRADED` deliberately remain flyable. An ordinary storage or
logging degradation appearing after arming does not change lifecycle state or
stop output. `UNKNOWN` is treated as lost safety knowledge: it blocks powered
output and enters `FAILSAFE` when currently armed; the next synchronization
frame is a valid DShot stop frame. `CRITICAL` normally means the fault system
synchronously entered terminal `FAULT`; terminal and backend failures retain
the independent hardware force-stop path.

An invalid command while armed enters `FAILSAFE` and immediately force-stops;
periodic stop-frame streaming resumes on the next healthy synchronization
release. Lease expiry enters `FAILSAFE` and replaces power with a stop frame in
that same service release. A
fresh command cannot resume output from `FAILSAFE`; the existing lifecycle
requires an explicit disarm before another arm request.

## Command acceptance and periodic output

`motor_control_submit()` first verifies its source, then validates and maps one
complete command, then
atomically replaces the retained physical-order command. Acceptance renews the
command lease but does not submit directly to the backend. Command-source and
USB response timing therefore cannot determine the DShot frame cadence.

`motor_control_synchronize()` must be called on every future motor-output
service release, even if no producer supplied a new command. It rechecks:

- lifecycle state;
- current health; and
- freshness of the retained command accepted from a producer;
- completion status of the preceding physical frame.

When armed checks pass and the backend is idle, the service submits one new
one-shot copy of the retained command. In `DISARMED`, `FAILSAFE`, or while
armed without a command, it instead submits a newly timestamped all-zero DShot
stop command. At the production 1 kHz task rate this continuously conditions
the ESCs with valid packets and repeats power commands every millisecond while
a slower USB/radio/control heartbeat renews only the power-command lease.
Physical retransmission never changes the retained power-command timestamp, so
producer silence still expires normally.

The current default command timeout is 100,000 microseconds. The production
`motor-control` task runs every 1,000 microseconds at
`TASK_PRIORITY_HIGHEST`, so detection adds at most one normal service period
beyond the timeout when the cooperative scheduler is meeting deadlines. It
also converts asynchronous DMA backend errors into critical faults in main
context. A transfer still busy 1,000 microseconds after submission is treated
as a stuck backend and stopped critically; the normal DShot300 table takes
approximately 60 microseconds. No interrupt calls this API.

## Force-stop and fault policy

Initialization begins with the generic motor-output facade's independently
accepted emergency stop. After the scheduler enters `DISARMED`, the 1 kHz task
starts periodic DShot stop frames. Initialization also verifies that every motor and
state-transition fault ID it depends on exists in the injected catalogue with
critical severity. Normal rejection avoids repeatedly calling the backend when
the private state already records an accepted stop.

Once output might be active, the following conditions override normal demand:

| Condition | Response |
| --- | --- |
| `DISARMED` or `FAILSAFE` | Repeated valid four-motor DShot stop frames |
| `BOOT`, `INITIALIZING`, or terminal `FAULT` | Hardware force stop |
| `UNKNOWN` health while armed | Enter failsafe; transmit stop frames |
| Invalid command | Enter failsafe; immediate force stop, then stop-frame streaming |
| Expired command lease | Enter failsafe; transmit stop frames |
| Mapping invariant failure | Critical motor-output fault; force stop |
| Backend submission error | Critical motor-output fault; force stop |
| Force-stop rejection/error | Critical force-stop fault; physical state remains explicitly unknown |

The catalogue owns three new critical IDs: motor initialization, motor output,
and motor force-stop. Critical reporting remains the existing fault
system's responsibility and synchronously drives lifecycle `FAULT`.

`force_stop` has no busy result. An accepted stop invalidates the retained
command and clears preparation readiness. A failed stop never sets the private
`outputs_stopped` flag, so the physical condition remains explicitly unknown;
its critical fault also moves the lifecycle to terminal `FAULT`.

## Arm admission

The shared application safety policy permits USB arming only for `OK`,
`WARNING`, or `DEGRADED` health. `UNKNOWN` and `CRITICAL` return a distinct
`health_rejected` response before an arm event reaches the state machine.
The state machine still independently requires the current lifecycle state to
allow an arm request. USB arming is also rejected with `motor_not_ready` until
the output task has transmitted stop frames for five seconds. This readiness
latches for normal arm/disarm cycles and is cleared by an emergency hardware
stop.

USB can currently enter logical `ARMED`. Its `motor_test` command selects one
logical motor from 1 through 4 and accepts the full normalized throttle range.
It remains a physically trusted development interface; propellers must be
removed for bench use.
Backend initialization and an accepted initial emergency stop are mandatory
during startup; normal disarmed operation then keeps DShot communication alive.

## Verification boundary

Host tests use an injected copying backend and cover initialization failure,
initial-stop failure, private mapping, command retention without direct output,
1 kHz-style repeated submissions, atomic replacement, inclusive lease expiry,
stuck-busy and backend errors, warning/degraded continuation, state and health
blocking, invalid commands, failsafe entry, critical fault reporting,
force-stop failure, and stopped-state tracking. Separate policy tests cover all
health values, while USB tests cover health-rejected arm requests.

These tests prove software decisions, callback ordering, table ownership, and
asynchronous error propagation. Firmware builds verify the selected register
interface. They do not prove GPIO voltage, DMA cancellation, timer preload
behavior, waveform timing, ESC recognition, motor response, or physical stop.
Those remain staged, propeller-free hardware checks.
