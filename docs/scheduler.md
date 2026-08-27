# Cooperative scheduler

Milestone 0.6 provides a deterministic, allocation-free scheduler driven by
the monotonic `time_us()` clock. It is cooperative: callbacks run to completion
and are never preempted by another task.

## Initialization

`scheduler_initialize()` receives a fixed-capacity task registry and a clock
function. Production passes `time_us`; host tests pass a controllable fake
clock. Every registered task keeps its enabled state, resets its runtime
statistics, and receives the scheduler start time as its first release. Tasks
are therefore eligible for one immediate startup execution.

## Ready batches and selection

When no previous ready work remains, the scheduler reads the clock once and
builds a bit mask containing every enabled task whose release has arrived:

```text
enabled && now_us >= next_release_us
```

That mask is a ready batch. Each task in the batch can be selected at most once
before the scheduler creates another batch. Within a batch, the scheduler
chooses:

1. lowest numeric priority;
2. earliest release time;
3. lowest registration order.

One call to `scheduler_run_once()` executes at most one callback. The
application repeatedly calls it from the main loop.

## Starvation behavior

The ready-batch boundary prevents a high-frequency, high-priority task from
re-entering ahead of a lower-priority task that was already ready. For example,
if high- and low-priority tasks are captured together, the high-priority task
runs first, its ready bit is removed, and the low-priority task must run before
a new batch can admit the high-priority task again.

This provides bounded selection fairness across at most 16 tasks, but it
cannot create CPU capacity. A callback that never returns blocks all tasks in
any cooperative scheduler. If total callback demand approaches or exceeds the
processor budget, lower-priority work will run but deadlines can still be
missed. Callbacks must therefore be bounded and non-blocking, and measured
execution/missed-release statistics must be used to validate system load.

A high-priority task that becomes ready after a batch snapshot waits until the
current finite batch drains. This is the deliberate fairness tradeoff; its
maximum additional delay is the sum of the bounded callbacks remaining in that
batch.

## Period advancement and missed releases

After a callback finishes, its release advances from the previous scheduled
release rather than from its finish time. This prevents accumulating phase
drift.

If the callback or other work made multiple periods late, the scheduler jumps
directly to the first release after the finish time:

```text
periods_to_advance =
    ((finish_us - previous_release_us) / period_us) + 1

next_release_us =
    previous_release_us + periods_to_advance * period_us
```

Skipped releases increment `missed_release_count`; callbacks are not invoked
repeatedly to catch up. This avoids a catch-up storm monopolizing the main loop.

## Measurements

Each execution records:

- start time;
- last execution duration;
- maximum execution duration;
- saturating execution count;
- saturating missed-release count;
- saturating overrun count.

An overrun means the callback execution duration is greater than its configured
period. A missed release means scheduling or execution crossed one or more
period boundaries. Separating these values distinguishes a slow callback from
a task delayed by other work. Durations greater than the 32-bit statistics
range saturate rather than wrap.

## Firmware diagnostics

The firmware always registers three no-hardware diagnostic tasks and, after a
successful USB/backend initialization, one bounded logging task:

| Task | Period | Frequency | Priority | Action |
| --- | ---: | ---: | ---: | --- |
| `diagnostic-fast` | 1,000 us | 1,000 Hz | High (64) | Increment debugger counter |
| `diagnostic-medium` | 10,000 us | 100 Hz | Normal (128) | Increment debugger counter |
| `diagnostic-slow` | 100,000 us | 10 Hz | Low (192) | Increment debugger counter |
| `logging-drain` | 1,000 us | 1,000 Hz | Background (255) | Advance USB state and attempt one record |

The diagnostic tasks expose approximate 100:10:1 execution ratios. The USB
task demonstrates that a high-frequency task may still remain lowest priority:
ready-batch fairness lets it run after higher-priority ready tasks, while its
callback remains bounded and never waits for a host.

## RTOS migration boundary

The task definition can later map to an RTOS backend as follows:

| Current field/behavior | RTOS mapping |
| --- | --- |
| callback and context | RTOS task entry and argument |
| period | periodic delay/deadline configuration |
| priority | mapped through an explicit RTOS priority table |
| enabled | suspended/resumed task state |
| execution statistics | runtime statistics and deadline monitoring |
| fixed registry | static task-control-block and stack allocation policy |

Such a migration would replace the cooperative backend, not change peripheral
or flight-control module contracts. Milestone 0.6 does not include an RTOS,
dynamic stacks, preemption, or interrupt-driven task dispatch.
