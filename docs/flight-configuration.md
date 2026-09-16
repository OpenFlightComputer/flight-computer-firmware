# Unified flight configuration

The firmware uses one versioned JSON document as the source for vehicle settings
that must be shared between firmware, command-line tools, and a future GUI.
There are intentionally no per-field mutation commands: clients read, edit,
validate, and write one complete snapshot.

## Default document

`config/default-flight-configuration.json` is the only production source for
compiled defaults. CMake validates its basic shape and generates C constants at
configure time. A new board, an explicitly reset board, or a mass-erased board
therefore starts with `PROPS_IN`, four `NORMAL` ESC direction settings, and the
reviewed receiver-failsafe values in that file. Schema 10 carries the startup gyro-calibration policy, the selected gyro
and accelerometer filters and cutoffs, and the selected attitude estimator, correction time
constant, and maximum accepted sample gap. It adds control-input deadbands,
angle/rate limits, maximum throttle, four bounded control-point curves,
roll/pitch attitude gains, and the three-axis rate-controller parameters. It
removes the former open-loop mixer factors; PID output limits now bound the
correction authority. It also adds explicit level-calibration policy and the
persisted roll/pitch mounting trim.

The motor array always uses logical aircraft order:

1. front-left;
2. rear-left;
3. front-right;
4. rear-right.

`propeller_layout` selects the mixer yaw convention. `directions` contains the
absolute DShot setting stored by each corresponding ESC. They are separate
fields because a wiring correction may intentionally differ from the nominal
layout. A configuration editor must update both consistently when changing the
vehicle convention.

Schema 10 adds the initial `easy_mode` policy. `armed_idle_throttle` is the
minimum output while receiver control is armed, while an exact zero throttle
stick still resets the rate controller and commands that equal idle value to
all motors. Disarming remains the only operation that commands a full motor
stop. `stabilization_activation_throttle` selects when the launch attitude is
allowed to begin moving toward level. The launch attitude is captured while
the shaped throttle is exactly zero, then frozen on its first nonzero value for
the remainder of that arm session. This prevents motion during spool-up from
silently redefining the launch reference even when throttle remains below the
activation threshold. `takeoff_leveling_rate_dps` bounds the transition;
disabling `takeoff_leveling_enabled` bypasses the launch-offset transition
without changing the rest of angle mode.

For the physically verified props-in layout, positive yaw correction raises
the counter-clockwise M2/M3 pair and lowers the clockwise M1/M4 pair. The
props-out layout uses the inverse diagonal. This sign is defined against the
standard body convention where positive yaw rotates the nose right.

## Runtime commands

```bash
./ofc config read
./ofc config read --output quad.json
./ofc config write quad.json
./ofc config reset
```

`read` returns only the active configuration object, making its output directly
usable by `write`. `write` accepts exactly the complete schema; missing,
duplicate, additional, invalid, or out-of-range fields are rejected. `reset`
erases the persistent override and reapplies the compiled JSON defaults.

Write and reset require lifecycle `DISARMED` with no pending arm. A successful
write is persisted before the active snapshot is replaced. Runtime application
updates all four motor directions, receiver freshness thresholds, receiver
failsafe policy, prepared input shaping and mixer signs, and the IMU processing
pipeline as one operation. Replacing
the filter or estimator configuration clears its history so the next fresh IMU
sample seeds a new estimate instead of mixing two configurations. Direction
changes start a ten-frame DShot configuration sequence, and all four directions
are reasserted again before every arm.

Gyro-calibration fields are startup policy. A write persists them with the
same complete document, and they take effect on the next boot; the bias itself
is deliberately RAM-only and is measured again on every power-up or reset.

Level calibration is explicit and separate from the startup gyro-bias pass.
`./ofc device imu calibrate-level` collects stationary samples on the mounted,
level aircraft and saves the measured roll/pitch reference only after all
checks pass. The correction is subtracted from accelerometer attitude before
the complementary estimator. Changing configuration resets estimator history.
The calibration duration and movement/plausibility limits are configurable;
the resulting `calibrated`, `roll_trim_degrees`, and `pitch_trim_degrees`
fields are part of the same whole-document snapshot. Defaults are uncalibrated,
and motor arming is blocked until a valid level calibration is active.

## Persistent storage

Flight Computer V1 reserves STM32F405 sector 11 at `0x080E0000` through
`0x080FFFFF`. The application linker region ends before it, so normal flashing
does not overwrite settings. A programmer mass erase still clears the sector.

The board layer stores a 512-byte versioned payload inside fixed 536-byte
append-only records. Each record has a format version, sequence, payload
length, CRC32, and a commit word programmed last. The sector holds 244 full
configuration records before an explicit reset is needed.

The loader migrates the 508-byte schema-9 payload by supplying the default
Easy-mode policy. It migrates the 500-byte schema-8 payload by supplying the default
accelerometer cutoff and integral-activation threshold. It also migrates the
488-byte schema-6/schema-7 payload by ignoring its
retired mixer-factor slots and marking level calibration incomplete. It can
also migrate the prior 480-byte schema-5 document, the 412-byte
schema-4 document, the 96-byte
schema-3 document, the 84-byte schema-2 document, the earlier 88-byte schema-1
document, and the eight-byte motor-direction payload. The storage layer still
recognizes the previous 120-byte record format. During migration it preserves
all fields that existed in the old payload and fills new fields from the
canonical JSON defaults.
Corrupt or unknown nonempty storage still fails startup closed.

## Control input configuration

Roll, pitch, and yaw each have a centered deadband, a maximum rate, and a
curve. Roll and pitch additionally have maximum angles for the later
self-leveling outer loop. Throttle has a zero deadband, a maximum output, and
its own curve. Each curve is described by two through eight normalized
`[input, output]` points. Each curve must begin at `[0,0]` and end at `[1,1]`;
inputs must increase and outputs may never decrease.

The initial `CONTROL_POINTS`/`LINEAR` implementation interpolates between
adjacent points. At configuration activation, firmware converts every segment
to a fixed set of polynomial coefficients. Runtime evaluation performs a
bounded scan of at most seven segments and one Horner polynomial evaluation.
The coefficient representation intentionally has room for quadratic and cubic
segments, so later interpolation types do not require changing the flight-loop
interface or JSON curve container.

The compiled defaults use a small 3% roll/pitch deadband, 4% yaw deadband,
30-degree roll/pitch limits, 180-degree-per-second roll/pitch limits, and a
150-degree-per-second yaw limit. Centered axes use a gentle three-point curve.
Throttle initially remains linear after its 2% zero deadband, with a maximum
of 100%, because a vehicle-specific lift-off plateau has not yet been measured.

## IMU processing configuration

The initial supported pipeline is intentionally small and explicit:

```json
"imu": {
  "gyro_filter": {
    "type": "FIRST_ORDER_LOW_PASS",
    "cutoff_hz": 80.0
  },
  "accelerometer_filter": {
    "type": "FIRST_ORDER_LOW_PASS",
    "cutoff_hz": 20.0
  },
  "attitude_estimator": {
    "type": "COMPLEMENTARY",
    "accelerometer_correction_time_constant_s": 0.5,
    "maximum_gap_us": 10000
  }
}
```

The string `type` fields are selections rather than hidden hard-coded
branches. Only the listed types are accepted today; future implementations can
add another module and selection without nesting it inside the acquisition
task. Filter and estimator changes take effect immediately after a successful
disarmed write. Startup-calibration settings still take effect on the next
boot because the accepted bias is deliberately immutable for that boot.

## Control-task relationship

The receiver-service task receives, parses, timestamps, checks freshness, and
normalizes CRSF data. It does not construct curves or calculate motor values.
A separate 1 kHz flight-control task reads that published snapshot, evaluates
arming and receiver-loss policy, applies the already-prepared deadbands and
curves, and then applies the already-prepared quad-X mixer. The existing
highest-priority 1 kHz motor-control task remains the only owner of DShot
submission.

Throttle at or below the configured zero deadband returns an exact all-zero
setpoint before roll, pitch, or yaw shaping is evaluated. USB motor tests and
the Stage 1 receiver failsafe are physical output requests rather than pilot
stick inputs, so they deliberately bypass pilot curves and deadbands.

The shaped controls now drive the stabilized path. The flight-control task
converts desired roll/pitch angles into desired rates, passes yaw through as a
desired rate, evaluates the rate PID, and sends the three PID corrections to
the quad-X mixer. The configured receiver Stage 1 fallback bypasses pilot
curves but uses the same stabilization path.

The mixer follows the documented body convention in which positive pitch is
nose-up. A positive pitch correction therefore increases the two front motors
and decreases the two rear motors; a negative correction does the reverse.
This sign was physically checked with the mounted vehicle because algebraic
mixer tests alone cannot establish the real front/rear orientation.

## Attitude-controller configuration

Schema 7 includes independent roll and pitch angle-controller gains, both `4.0
s^-1` by default. A roll error of 10 degrees consequently requests 40 degrees
per second, bounded by the existing per-axis `maximum_rate_dps` setting. The
flight-control task calls the roll, pitch, and yaw modules directly. Roll and
pitch each convert angle error into a rate request. Yaw currently passes the
pilot's requested yaw rate through because the vehicle has no magnetometer or
other absolute-heading source. There is no generic controller type, axis-mode
enum, function-pointer dispatch, or coordinator object.

## Rate-controller configuration

The rate-controller configuration includes one selected controller type, a maximum accepted IMU sample
gap, and independent roll, pitch, and yaw PID settings. Each axis stores `kp`,
`ki`, `kd`, `integral_limit`, and `output_limit`. Limits are normalized mixer
corrections in the range zero through one. The compiled gains are deliberately
conservative starting values and have not yet been tuned or physically
validated on the aircraft.

Schema 9 adds `integral_activation_throttle`, initially `0.2`. Below that
shaped throttle the I terms are cleared while P and D remain active. When the
mixer reports saturation, integration is held on the following cycle until
headroom returns. This prevents low-throttle and actuator-limited operation
from accumulating a correction that is impossible to apply.

The hardware-independent controller derives `dt` from consecutive IMU
acquisition timestamps. Its derivative acts on measured gyro rate, so a
setpoint step does not create a derivative kick. Conditional integration and
an explicit integral clamp prevent windup while still allowing an opposing
error to unwind a saturated controller. Duplicate, reversed, invalid, or too
widely separated samples produce no correction and reset continuity where
required. Disabling control resets all accumulated state.

The existing 1 kHz flight-control task calls the three outer-axis modules and
the inner rate controller. It resets PID history at zero throttle, failsafe,
lost authority, or lost/invalid IMU data. Fresh duplicate or briefly stale IMU
data does not create another command; the motor layer retains the last complete
accepted command. Lost or incoherent IMU data enters the central failsafe.

While armed in Easy mode, exact zero normalized throttle bypasses axis work and
commands the configured equal motor idle. For nonzero throttle, the configured
idle is the lower motor floor and pilot throttle is rescaled across the
idle-to-full range. The mixer applies a prepared pure-sign quad-X matrix to the
three PID corrections and scales all corrections equally until they fit the
headroom between that floor and full output. It never shifts collective power,
so stabilization cannot force a motor below armed idle. Disarming bypasses this
policy and the motor safety layer sends four exact-zero stop commands. Schema 7
removed the obsolete open-loop mixer factors because the PID output limits
already bound correction authority. The result still passes through the
central motor lifecycle, source, health, freshness, mapping, and backend gates.

Stage 2 receiver loss now enters the central `FAILSAFE` state immediately from
the flight-control task. The motor task emits stop frames on its next release;
it does not wait for the retained-command lease to expire. Once fresh receiver
input holds arm low and throttle below the configured recovery limit for the
configured recovery interval, the same task explicitly returns the lifecycle
to `DISARMED` (or confirms it is already there) and releases the Stage 2 latch.
A new low-to-high arm-switch edge is still required to arm again.

## Verification boundary

Native tests cover JSON-derived defaults, curve preparation and interpolation,
deadbands, armed-idle early return, maximum-curve serialization, legacy
migration, disarmed-only replacement/reset, runtime application including
prepared shaping/mixer and processing-pipeline replacement, stabilized mixer
equations, floor-aware saturation handling, armed-idle behavior, IMU gates,
source ownership, and immediate Stage 2 failsafe entry. Debug and Release
cross-builds verify the generated header and flash integration.
