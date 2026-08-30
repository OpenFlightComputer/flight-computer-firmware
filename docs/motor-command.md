# Motor command model

Milestone 1.1 introduces the hardware-independent command passed from future
manual or flight-control producers toward the actuator subsystem. It does not
encode DShot, access hardware, authorize output, or store a global command.

## Representation

`motor_command_t` owns one atomic snapshot:

```c
typedef struct {
    float throttle[4];
    uint64_t timestamp_us;
    bool valid;
} motor_command_t;
```

Array indices zero through three mean physical labels Motor 1 through Motor 4.
Exact pin, timer, vehicle-position, and rotation-direction mappings remain later
review points; this milestone establishes only stable logical numbering.

Each throttle is normalized:

| Value | Meaning |
| --- | --- |
| `0.0f` | stopped |
| greater than `0.001f` through `1.0f` | valid normalized throttle |
| below `0.0f`, above `1.0f`, NaN, or infinity | invalid input |

Values from exact zero through the inclusive `0.001f` stop threshold are stored
as exact `0.0f`. This prevents a small floating-point residue from becoming a
future minimum nonzero DShot value. The threshold is a reviewed software
starting point and must be checked against ESC behavior during bench validation.

A valid all-zero snapshot is the ordinary stop representation; there is no
separate stop-command variant. `valid == false` means no usable snapshot exists
and future output policy must stop.

## Creation and rejection

`motor_command_create()` validates all four input values into a local candidate
before changing the destination. Success stores all four values, the supplied
monotonic timestamp, and validity together. Failure leaves the prior destination
unchanged and therefore does not refresh its timestamp.

This behavior permits a consumer to keep using the last valid snapshot only
until its bounded timeout. Repeated invalid submissions cannot keep it fresh.
No clamping is performed above the stop threshold: a value outside the
normalized range indicates a producer error and rejects the complete snapshot.

`motor_command_initialize()` and `motor_command_invalidate()` clear every
throttle, the timestamp, and validity. Neither operation allocates memory.

## Freshness

`motor_command_is_fresh()` requires all of the following:

- a non-null valid command;
- a nonzero timeout;
- `now_us` not earlier than the command timestamp; and
- elapsed time less than or equal to the timeout.

The default proposed Phase 1 timeout is 100,000 microseconds. It is exposed as a
constant but passed explicitly to the freshness function so a later actuator
policy owns the configured timeout. Exactly the timeout boundary is fresh; the
next microsecond is stale. A timestamp in the future is rejected instead of
allowing unsigned subtraction to appear fresh.

Freshness does not inspect system state or health. The later final safety gate
must independently require permitted lifecycle/health state and a fresh command.

## Floating-point contract

Production and host builds compile-time check for a 32-bit binary float with a
24-bit significand, matching IEEE-754 single precision. Production uses the
STM32F405 Cortex-M4F single-precision FPU and hard-float ABI. Constants use the
`f` suffix, and `isfinite()` is evaluated before range comparisons so NaN cannot
pass through the usual comparison behavior.

The model deliberately contains no USB number parser. Milestone 1.9 may choose
a decimal or fixed-unit wire representation and must translate it through this
same creation function.

## Ownership and current boundary

The structure is a value snapshot and contains no synchronization. Phase 1
producers and consumers must define ownership before sharing it across main and
interrupt contexts. Current firmware creates no instance and produces no motor
output; the module is compiled only to establish the portable contract.
