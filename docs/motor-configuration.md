# Persistent motor direction configuration

Milestone 3.3 adds a disarmed-only configuration path for the direction stored
by each ESC. Direction is indexed by logical motor 1 through 4, so it remains
attached to the aircraft position even if a board revision changes timer or
pin routing.

## Configuration model

`motor_configuration_t` contains one absolute `NORMAL` or `REVERSED` value for
each logical motor. The compiled defaults are all `NORMAL` and live in
`flight/actuators/motor_configuration.c`. These defaults are the fallback for
a new board, after an explicit reset, or after a mass erase.

A successful runtime change is written before the in-memory configuration is
replaced. A failed write therefore leaves the previous configuration active.
Changes and reset are rejected unless the lifecycle is exactly `DISARMED` and
no arm operation is pending.

The setting changes the direction stored by the ESC. It is distinct from the
logical-to-physical motor mapping and from the future mixer's expected CW/CCW
aircraft convention.

## DShot application sequence

The DShot backend maps `NORMAL` to command 20 and `REVERSED` to command 21.
The request bit is set as required for settings commands. All four configured
directions are mapped into physical ESC order and sent together, one complete
frame per 1 kHz motor-control release.

After a successful configuration change or reset, the controller starts a
ten-frame direction sequence immediately. On every arm request it starts the
same sequence again. The lifecycle remains `DISARMED`, with only an internal
pending source, until all ten frames have completed. The common state machine
then enters `ARMED` and latches that source as the active owner. A receiver arm
is cancelled if the switch returns low or receiver input becomes unusable
during this pending interval.

This reasserts the FC-owned configuration at every arm. Unidirectional DShot
does not provide readback, so software cannot prove that an ESC accepted the
setting; propeller-free direction observation is still required.

## Persistent storage

Flight Computer V1 reserves STM32F405 flash sector 11 at `0x080E0000` through
`0x080FFFFF` exclusively for configuration. The application image link region
ends before this sector, so a normal firmware image does not contain or
overwrite configuration bytes. A programmer mass erase still clears them.

The board storage implementation uses fixed-size append-only records with a
format version, sequence, payload length, CRC32, and a commit word programmed
last. A partial write is ignored when an earlier valid record exists. If the
sector contains data but no compatible valid record, startup fails closed
rather than silently selecting potentially wrong directions.

The sector holds 3,276 records. It is not erased automatically when full;
another change then returns `configuration_storage_error`. The explicit reset
erases the sector and returns to the compiled defaults.

## USB and host commands

Inspect the active configuration:

```bash
./ofc motor direction show
```

Set one logical motor to an absolute direction:

```bash
./ofc motor direction set --motor 3 --direction reversed
./ofc motor direction set --motor 3 --direction normal
```

Erase the persistent override and restore compiled defaults:

```bash
./ofc motor configuration reset
```

Responses report `source` as `DEFAULT` or `PERSISTENT` and always include all
four effective directions. There is intentionally no toggle operation, no
armed update, and no ESC `SAVE_SETTINGS` command: the FC remains the source of
truth and reasserts its value on every arm.

## Verification boundary

Native tests cover defaults, validation, storage serialization/versioning,
write and erase failures, disarmed gating, logical-to-physical reordering,
exact DShot command frames, ten repetitions, delayed arm completion, USB
schemas, and pending receiver-arm cancellation. Debug and Release cross-builds
verify the reserved flash layout and STM32 flash API. Persistence across power
cycles, behavior across normal flashing, and the final direction of each
physical motor remain propeller-free hardware checks.
