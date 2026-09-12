# Flight-control diagnostics

Milestone 4.9 adds one bounded diagnostic path for the final propeller-free
control validation. It observes values the production flight-control task
already computes; it does not add another scheduler task, rerun control math,
or construct JSON in the high-priority path.

## Capture model

`control_trace` owns a fixed 64-record RAM ring and is off after every reset.
Capture can only be started while `DISARMED`; once started it may continue
through `ARMED` and `FAILSAFE`, then stops automatically on `DISARMED` or
`FAULT`. Recording never waits for USB and never overwrites an unread record.
When the ring is full, the new record is dropped and the saturating
`dropped_records` counter makes that loss visible.

| Level | Periodic samples | Intended use |
| --- | ---: | --- |
| `EVENTS` | none | State, failsafe, controller-result, and mixer-saturation transitions |
| `LOW_RATE` | 10 Hz | Long, light-weight observation |
| `HIGH_RATE` | 100 Hz | Default live validation |
| `FULL_RATE` | 1 kHz | Short RAM trace for timing-sensitive behavior |

Events are retained at every enabled level in addition to periodic samples.
Each record contains receiver controls, the shaped setpoint, estimated
roll/pitch, desired and measured rates, per-axis P/I/D/total corrections,
four final motor commands, mixer scale/shift/saturation, lifecycle and
authority, failsafe, IMU freshness, timestamps, sequences, and validity flags.

The flight-control coordinator returns its ordinary cycle result to the task.
The task gives that result and the already-published receiver/IMU/controller
snapshots to the trace module. This keeps control algorithms independent of
diagnostics and confines the trace-enabled check to the task boundary.

## USB ownership and backpressure

The background USB task reads records in bounded chunks. Firmware serializes
compact integer milli-units under the existing 4,096-byte response limit; the
host restores floats for presentation. A read first peeks at RAM. Records are
discarded only after the complete response is accepted by the USB transmit
queue. A busy transport retries the identical response and therefore cannot
silently consume trace data.

Wire schema 1 represents each record as this ordered array; nested numeric
values are signed integers divided by the response's `scale` (currently 1000):

```text
[sequence, timestamp_us, imu_sequence, event_flags,
 state, source, failsafe_state, failsafe_action, imu_freshness,
 control_result, rate_result, validity_flags, dt_us,
 [receiver_throttle, receiver_roll, receiver_pitch, receiver_yaw],
 [setpoint_throttle, desired_roll_angle, desired_pitch_angle, desired_yaw_rate],
 [attitude_roll, attitude_pitch],
 [desired_roll_rate, desired_pitch_rate, desired_yaw_rate],
 [measured_roll_rate, measured_pitch_rate, measured_yaw_rate],
 [roll_P, roll_I, roll_D, roll_total,
  pitch_P, pitch_I, pitch_D, pitch_total,
  yaw_P, yaw_I, yaw_D, yaw_total],
 [motor_1, motor_2, motor_3, motor_4],
 mixer_scale, collective_shift, mixer_saturated]
```

Validity bits 0 through 4 correspond to receiver, setpoint, attitude, rate
output, and mixer output. Enum values follow their firmware declarations; the
Python decoder rejects values it does not understand instead of mislabelling
them.

`FULL_RATE` can fill the ring in 64 ms. It is deliberately a short RAM capture,
not a promise that USB can stream every 1 kHz sample indefinitely. The dropped
counter is the authority for deciding whether a capture is complete enough.

## CLI

Start the default 100 Hz live view while disarmed, then arm with the receiver:

```bash
./ofc device control --watch
```

Select another capture level or export all received records:

```bash
./ofc device control --watch --level low
./ofc device control --watch --level full --output control-trace.json
./ofc device control --watch --output control-trace.csv
```

The Rich dashboard draws an attitude horizon, the four Quad-X motor positions
and outputs, desired/measured rate histories, PID terms, mixer saturation,
lifecycle/authority, failsafe, IMU freshness, and buffer drops. Animation and
history live entirely on the computer. A request without `--watch` drains and
displays an already-started or automatically-stopped capture without starting
a new one.
