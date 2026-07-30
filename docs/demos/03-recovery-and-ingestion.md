# Demo 3: offline recovery and isolated ingestion

Interview time: 10–12 minutes.

## Claim

Controller acceptance is transactionally recoverable across restart and lost
acknowledgements, while untrusted PDF work is handled in a separate bounded,
metadata-only process boundary.

## Run

```sh
cmake --preset dev
cmake --build --preset dev
cd apps/diagnostics
npm ci
npm run scenario
```

Point out the active-session restart, accepted duplicate, rejected sequence
conflict, CRC rejection, acknowledgement failure, duplicate transmission with
one receiver effect, second restart, and final queue drain.

For the worker:

```sh
cd ../../services/ingestion
.venv/bin/python -m pytest \
  -k "deadline or idempotency or shell_false or redacts"
```

## Narrate

- The acceptance transaction includes the checkpoint, audit, queue, sequence,
  and replay record.
- Remote receipt followed by failed local acknowledgement is intentionally
  at-least-once; stable IDs suppress duplicate receiver effects.
- The PDF service streams into a private temporary directory, sends only fixed
  arguments to a resource-limited helper, persists metadata rather than bytes
  or text, and cleans up on success/failure.

## Expected evidence

The scenario reports 15 passing steps, ends with an empty queue and six receiver
effects, and explicitly demonstrates duplicate/conflict/malformed decisions.
Selected Python failure-path tests pass.

Do not claim exactly-once delivery, a complete PDF sandbox, or distributed
idempotency.
