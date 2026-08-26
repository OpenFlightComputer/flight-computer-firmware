# Task abstraction

Milestone 0.5 defines a task as a lightweight C data structure describing one
periodic unit of cooperative work. It does not execute callbacks or implement
the scheduler.

## Definition

A `task_definition_t` contains:

- a unique, non-empty name of at most 31 characters;
- a nonzero period in integer microseconds;
- an 8-bit priority;
- a callback;
- an optional caller-owned context pointer passed to the callback.

Definitions are written in C rather than loaded from JSON or YAML. The
registry copies the definition, but it retains the name and context pointers.
Their referenced storage must therefore remain valid for the registered
task's lifetime; string literals and static objects are the intended inputs.

Periods are stored rather than frequencies because the future scheduler will
compare microsecond timestamps directly. For example, a 1,000 microsecond
period describes a 1 kHz task.

## Priority convention

Priority uses an unsigned 8-bit value where lower numbers represent higher
priority. Named reference values are provided:

| Name | Value |
| --- | ---: |
| `TASK_PRIORITY_HIGHEST` | 0 |
| `TASK_PRIORITY_HIGH` | 64 |
| `TASK_PRIORITY_NORMAL` | 128 |
| `TASK_PRIORITY_LOW` | 192 |
| `TASK_PRIORITY_BACKGROUND` | 255 |

Values between these reference points remain available. Milestone 0.6 will
select the lowest numeric priority among ready tasks, then use the earliest
release and registration order to make ties deterministic. Task priority does
not change STM32 interrupt priority and will not preempt a callback already in
progress.

## Runtime metadata

Registration initializes each `task_t` as enabled and records:

- deterministic registration order;
- next release time;
- last start time;
- last and maximum execution duration;
- execution count;
- overrun count.

Scheduling and measurement fields start at zero. Milestone 0.6 owns the rules
for updating them.

## Fixed-capacity registry

`task_registry_t` contains storage for 16 tasks and never allocates memory at
runtime. Registration rejects null inputs, invalid names, zero periods, null
callbacks, duplicate names, and attempts beyond capacity. It preserves the
original registration order and provides bounds-checked indexed access.

The capacity is a compile-time limit rather than a target task count. Changing
it remains an explicit memory-budget decision.

## Milestone boundary

This milestone does not instantiate an application registry, execute a task,
read `time_us()`, select by priority, calculate deadlines, or detect overruns.
Those behaviors belong to the cooperative scheduler in Milestone 0.6.
