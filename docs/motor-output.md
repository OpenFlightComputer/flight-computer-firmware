# Generic motor-output interface

Milestone 1.2 defines the hardware-independent boundary between normalized
four-motor commands and a replaceable physical-output backend. It does not
select DShot, configure hardware, authorize output, or store a latest command.

## Data flow and dependency boundary

```text
manual/control producer
        ↓ motor_command_t
motor_control safety gate
        ↓
motor_output facade
        ↓ backend callbacks
application-owned backend adapter
        ↓
DShot, PWM, CAN, fake, or other lower implementation
```

The facade accepts only `motor_command_t`, so flight producers do not know
protocol values, timer channels, DMA buffers, routed pins, or ESC timing. The
backend is injected as three callbacks plus an opaque context pointer.

To preserve `app -> flight -> peripherals -> hardware` dependency direction, a
production adapter that understands both this facade and a selected
peripheral belongs in the application integration layer, like the existing USB
logging adapter. A DShot peripheral implementation must not include this
flight-layer header or acquire knowledge of `motor_command_t`.

## Backend contract

```c
typedef struct {
    motor_output_backend_initialize_t initialize;
    motor_output_backend_submit_t submit;
    motor_output_backend_force_stop_t force_stop;
    void *context;
} motor_output_backend_t;
```

All callbacks are required. The context may be null if that backend does not
need it. The facade copies the descriptor after successful initialization, so
the caller's descriptor may leave scope; the object referenced by a non-null
context must remain alive for the entire facade lifetime.

There is deliberately no deinitialization operation. The embedded output
subsystem has boot lifetime, and safety shutdown uses force-stop rather than
tearing down hardware resources.

## Initialization

`motor_output_initialize()` first clears the facade, validates every callback,
and asks the backend to initialize. A real backend's initialization must begin
from and retain a safe non-driving or stopped state while it prepares its owned
resources. The facade then invokes force-stop as an independent operational
check. Only both accepted results make the facade initialized.

An initialization or initial-stop error leaves the facade unusable and is
translated by `motor_control` into a critical catalogue-owned fault. Unknown
backend enum values fail as errors. No normal submission callback occurs during
initialization.

## Complete-command submission and ownership

`motor_output_submit()` accepts one complete four-motor snapshot rather than
per-motor updates. This prevents the backend from observing a mixture of new
and old motors while a producer updates a control result.

The facade does not trust public structure fields blindly. It requires
`valid == true` and recreates a canonical local command through the Milestone
1.1 validator. This rejects non-finite/out-of-range values and again converts
the stop threshold to exact zero without modifying the caller's object.

The backend receives the address of this facade-owned local snapshot, never the
caller's pointer. The callback has only the duration of that call to inspect it:

- `ACCEPTED` guarantees the backend copied every required value into its own
  fixed-lifetime storage before returning;
- `BUSY` and `ERROR` guarantee the backend retained nothing; and
- the caller may immediately modify or release its original command after any
  result.

This contract prevents stack lifetime errors, shared mutable commands, and DMA
reading data while a producer changes it. A future asynchronous DShot backend
will satisfy it by copying or converting the complete snapshot into an inactive
backend-owned frame buffer before returning accepted.

The facade maps accepted, busy, error, and unknown backend results to explicit
outcomes. It does not retry or queue commands; `motor_control` retains only
the last actually accepted command for periodic freshness enforcement.

## Force-stop semantics

`motor_output_force_stop()` is an unconditional output operation, not a special
`motor_command_t`. The backend stop contract must discard or override pending
normal demand and ensure that an older command cannot become active after the
stop request. It has no busy result: the backend either accepts responsibility
for stopping or reports an error requiring safety/fault handling.

The generic facade can enforce the result vocabulary but cannot prove the
physical backend stopped. The later DShot implementation and propeller-free
bench validation must verify cancellation, buffer, DMA, timing, and electrical
behavior.

Normal submission is permitted again after an accepted force-stop because this
layer owns mechanism, not lifecycle authorization. The Milestone 1.7
`motor_control` gate is the sole permitted production caller and decides
whether a submission is permitted.

## Current exclusions

The facade itself still has no global instance, task, state/health lookup,
freshness enforcement, fault report, DShot representation, timer/DMA/GPIO
configuration, USB command, or physical output. The application gate owns the
one production instance without changing this lower interface's responsibilities.
