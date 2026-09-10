# Unified flight configuration

Phase 3 uses one versioned JSON document as the source for vehicle settings
that must be shared between firmware, command-line tools, and a future GUI.
There are intentionally no per-field mutation commands: clients read, edit,
validate, and write one complete snapshot.

## Default document

`config/default-flight-configuration.json` is the only production source for
compiled defaults. CMake validates its basic shape and generates C constants at
configure time. A new board, an explicitly reset board, or a mass-erased board
therefore starts with `PROPS_IN`, four `NORMAL` ESC direction settings, the
initial mixer factors, and the reviewed receiver-failsafe values in that file.
Schema 3 also carries the startup gyro-calibration policy, the selected gyro
filter and cutoff, and the selected attitude estimator, correction time
constant, and maximum accepted sample gap.

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
failsafe policy, and the IMU processing pipeline as one operation. Replacing
the filter or estimator configuration clears its history so the next fresh IMU
sample seeds a new estimate instead of mixing two configurations. Direction
changes start a ten-frame DShot configuration sequence, and all four directions
are reasserted again before every arm.

Gyro-calibration fields are startup policy. A write persists them with the
same complete document, and they take effect on the next boot; the bias itself
is deliberately RAM-only and is measured again on every power-up or reset.

## Persistent storage

Flight Computer V1 reserves STM32F405 sector 11 at `0x080E0000` through
`0x080FFFFF`. The application linker region ends before it, so normal flashing
does not overwrite settings. A programmer mass erase still clears the sector.

The board layer stores a 96-byte versioned payload inside fixed 120-byte
append-only records. Each record has a format version, sequence, payload
length, CRC32, and a commit word programmed last. The sector holds 1,092 full
configuration records before an explicit reset is needed.

The loader can migrate the prior 84-byte schema-2 document, the earlier
88-byte schema-1 document, and the eight-byte motor-direction payload. During
migration it preserves all fields that existed in the old payload and fills
new fields from the canonical JSON defaults. Corrupt or unknown nonempty
storage still fails startup closed.

## IMU processing configuration

The initial supported pipeline is intentionally small and explicit:

```json
"imu": {
  "gyro_filter": {
    "type": "FIRST_ORDER_LOW_PASS",
    "cutoff_hz": 80.0
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
normalizes CRSF data. A separate 1 kHz flight-control task reads that published
snapshot, evaluates arming and receiver-loss policy, and applies the pure
quad-X mixer. The existing highest-priority 1 kHz motor-control task remains the
only owner of DShot submission.

The mixer returns four exact zeros immediately when normalized throttle is
exactly zero. Otherwise it scales roll, pitch, and yaw by the configured
factors, applies the selected propeller-layout yaw sign, calculates all four
logical motor values, and clamps each output to `0.0..1.0`. The result still
passes through the central motor lifecycle, source, health, freshness, mapping,
and backend gates.

Stage 2 receiver loss now enters the central `FAILSAFE` state immediately from
the flight-control task. The motor task emits stop frames on its next release;
it does not wait for the retained-command lease to expire. Once fresh receiver
input holds arm low and throttle below the configured recovery limit for the
configured recovery interval, the same task explicitly returns the lifecycle
to `DISARMED` (or confirms it is already there) and releases the Stage 2 latch.
A new low-to-high arm-switch edge is still required to arm again.

## Verification boundary

Native tests cover JSON-derived defaults, configuration validation,
serialization, legacy migration, disarmed-only replacement/reset, runtime
application including processing-pipeline replacement, mixer equations,
exact-zero handling, clamping, source ownership, and immediate Stage 2
failsafe entry. Debug and Release cross-builds verify the generated header and
flash integration.
