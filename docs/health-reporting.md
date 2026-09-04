# Structured health reporting

Milestone 0.12 exposes the fault system's current state as a bounded,
machine-readable USB response. Health is a read-only projection: it does not
detect problems, classify fault IDs, clear records, change lifecycle state, or
authorize arming.

## Authority and data flow

```text
subsystem detects a condition
        ↓ reports a fault ID
fault catalogue owns severity and source
        ↓
fault system owns active records and critical transition policy
        ↓ read-only scan
health evaluator derives one overall state and severity counts
        ↓
USB adapter serializes the bounded response
```

The fault system remains the source of truth. Logs remain a lossy event stream,
while `status` reports lifecycle state and uptime. Health answers the separate
question: what active fault information does the firmware currently know?

## Overall health derivation

The evaluator scans the fixed 16-record fault registry once. The first matching
rule wins:

| Condition | Overall health |
| --- | --- |
| Lifecycle state is `FAULT` | `CRITICAL` |
| At least one active critical record | `CRITICAL` |
| At least one fault record was dropped without terminal lifecycle state | `UNKNOWN` |
| At least one active ordinary fault | `DEGRADED` |
| At least one active warning | `WARNING` |
| Otherwise | `OK` |

Lifecycle `FAULT` overrides incomplete diagnostics because the critical safety
outcome is known even if its record could not be retained. Exhausting all 16
fault slots now synchronously requests that terminal state, regardless of the
severity of the record that could not be stored. Consequently a registry
overflow reached through the public fault-reporting API produces `CRITICAL`,
not `UNKNOWN`. The `UNKNOWN` branch is retained defensively for an externally
constructed or inconsistent snapshot in which lost data is visible but the
required lifecycle transition did not occur; normal operation must not remain
in that condition.

Occurrence count, timestamps, source, and context do not alter severity. They
explain a record but never reclassify it. Clearing a warning or ordinary fault
can improve the next health result. Critical records remain reset-latched, and
the cumulative dropped-record indication also remains until reset.

## USB schema

The existing `health` command now returns:

```json
{
  "type": "response",
  "request_id": 42,
  "command": "health",
  "ok": true,
  "health": "DEGRADED",
  "state": "DISARMED",
  "fault_data_complete": true,
  "active_fault_count": 1,
  "warning_count": 0,
  "fault_count": 1,
  "critical_count": 0,
  "dropped_fault_count": 0,
  "faults": [
    {
      "id": 10,
      "severity": "FAULT",
      "source": "APPLICATION",
      "occurrence_count": 1,
      "first_timestamp_us": 1200,
      "last_timestamp_us": 1200,
      "context": null
    }
  ],
  "reported_fault_count": 1,
  "truncated": false
}
```

The top-level `ok` reports that the command was parsed and answered; it does
not mean the vehicle is healthy. The authoritative diagnostic outcome is the
`health` field.

Timestamps and context are JSON `null` when their validity flags are false.
Context remains a catalogue/subsystem-owned numeric diagnostic; the health
layer does not assign it a new meaning.

Two completeness indicators intentionally describe different boundaries:

- `fault_data_complete` is false when the fault registry previously lacked a
  slot and dropped a record. The firmware does not know the missing detail.
- `truncated` is true when all active records exist internally but do not fit in
  this one USB response. Counts still describe the complete retained registry,
  while `reported_fault_count` describes only the serialized array.

## Bounds and ordering

Evaluation examines at most the fixed 16 slots and uses no allocation. This
scan occurs only when a `health` command is processed, not on every scheduler
tick. Serialization visits active slots in stable registry order and adds only
complete fault objects that fit beside the mandatory response suffix. The complete output
must remain below the existing 768-byte USB transmit-entry capacity. If the
next object cannot fit, it and all later objects are omitted and `truncated`
is set. No partial JSON object is emitted.

The health command still runs at most once per 1 ms `usb-service` release and
retains the complete response across transport backpressure. All evaluation
and formatting remains in cooperative main context, never the USB interrupt.

## Safety boundary

Health labels remain a read-only diagnostic projection rather than owning the
motor gate. The current state machine continues to decide transitions, and
the fault system continues to
send `FAULT_DETECTED` synchronously for critical reports and registry
exhaustion. Milestone 1.7's separate application safety policy explicitly
allows arm admission and output for `OK`, `WARNING`, and `DEGRADED`, while
rejecting `UNKNOWN` and `CRITICAL`. `UNKNOWN` appearing while armed also causes
the motor gate to enter `FAILSAFE` and force stop. Health evaluation itself
still performs no transition or output operation.
