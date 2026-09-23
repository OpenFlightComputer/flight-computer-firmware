---
name: ofc-blackbox-download
description: Download and decode OpenFlightComputer SD-card blackbox logs over USB into the project's ignored historic_logs directory when the user asks to inspect flight data.
---

# OpenFlightComputer Blackbox Download

Run this skill from the `flight-computer-firmware` repository root. It is a
read-only retrieval workflow: never initialize, clear, overwrite, or delete
the SD-card catalog as part of downloading a log.

## Safety and scope

Before attaching USB to an aircraft, use the user's explicit current-turn
confirmation that the battery is disconnected (or that the aircraft is
otherwise physically safe). Do not arm the aircraft, spin motors, flash
firmware, or change configuration. Props should remain removed for bench work.

The project keeps retrieved artifacts in `historic_logs/`, which is ignored by
Git. Preserve both the raw on-card bytes (`.ofcb`) and the decoded JSON; they
are useful for comparing firmware revisions and diagnosing flight behavior.

## Download workflow

If the user names a log ID, skip catalog selection and download that numeric
ID. Otherwise, first list the catalog:

```bash
UV_CACHE_DIR=/private/tmp/ofc-uv-cache ./ofc flight-log list
```

Select the highest-ID entry whose descriptor is complete. Do not blindly use
`latest` when the newest catalog entry is incomplete or has zero samples; an
interrupted recording may still be the newest entry. If no complete entry is
available, report that instead of attempting a download.

Choose a non-existing pair of output paths, normally:

```text
historic_logs/flight-log-<ID>.ofcb
historic_logs/flight-log-<ID>.json
```

If either path already exists, append a short `-copyN` suffix rather than
overwriting the prior evidence. Then run one download command:

```bash
UV_CACHE_DIR=/private/tmp/ofc-uv-cache ./ofc flight-log download <ID> \
  --output historic_logs/flight-log-<ID>.ofcb \
  --json-output historic_logs/flight-log-<ID>.json
```

The CLI retrieves the bounded sector stream, validates block CRCs, preserves
the versioned raw format, and decodes the JSON. A USB-device approval may be
needed; reuse the narrowly scoped prefix `['./ofc', 'flight-log']` rather than
requesting broad filesystem or device access.

## Completion report

Treat the operation as successful only when the command exits with status 0.
Report the selected log ID, the two absolute output paths, sample count, drop
count, final state, and firmware/build ID from the decoded JSON/footer. Mention
if the catalog contained newer incomplete entries and that the complete entry
was selected deliberately. If a download fails, retain any partial file for
inspection, report the failed stage, and do not clear or reinitialize the card.

Do not commit `historic_logs/` or alter unrelated working-tree changes.
