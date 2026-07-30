# AI session: transactional persistence and recovery

- Date: 2026-07-30
- Goal: implement the SQLite acceptance boundary, bounded outbound queue,
  restart recovery, capped reconnect retries, receiver idempotency, fault
  injection, and a JSON-lines simulator CLI.
- Human specification: Task 0001, Commit 4 acceptance criteria.
- AI tool: OpenAI Codex.

## Approach and risks

Codex proposed a one-writer store interface and a transaction batch containing
the checkpoint, audit entries, outbound frames, next sequence, and replay
record. The runtime remains an adapter around the pure reducer.

Principal risks were marking authenticated input before its effect committed,
silently dropping state at queue capacity, conflating logical saturation with
disk failure, retry loops after reconnect, and claiming exactly-once delivery.

## Suggestions

- Accepted: a bounded transactional batch, stable event identifiers, persisted
  retry metadata, fault-injecting adapters, and SQLite triggers for append-only
  audit behavior.
- Modified: the retry budget resets on a new connectivity-restored event rather
  than running indefinitely. Within one connection it is capped at five
  attempts with explicit backoff.
- Rejected: no implementation suggestion was rejected in this phase.

## Verification

- The warnings-as-errors developer build completed and the complete C++ suite
  passed 33 of 33 tests in 1.40 seconds.
- The simulator was run as an external process against a temporary database.
  Boot, occupancy, queued telemetry, and restart returned valid JSON lines and
  the reconstructed state and queue matched the pre-restart values.
- ASan and UBSan rebuilt all product targets and passed 33 of 33 tests in 6.00
  seconds without a sanitizer report.
- The first clang-tidy run found an implicit multiplication width, unnecessary
  path copies, an empty rollback catch, easily swappable integer parameters,
  and unchecked optional access. The API was strengthened with `RetryState`
  and `TelemetryMetadata` value objects, rollback was made non-throwing, path
  parameters became references, and optional handling was made explicit. The
  final product clang-tidy build passed with warnings treated as errors.

## Behavioral evidence

Tests exercise restart reconstruction, in-order reconnect drain, lost local
acknowledgements with idempotent receiver behavior, the five-attempt retry cap,
queue-full session blocking, failed transaction rollback, replay/duplicate/
conflict decisions across restart, replay-domain atomicity, append-only audit
triggers, and deterministic sensor noise.

## Residual risk

SQLite durability still depends on filesystem and hardware behavior. Append-only
triggers do not defend against privileged database replacement. Stable event
identifiers permit receiver idempotency but do not create exactly-once delivery.
