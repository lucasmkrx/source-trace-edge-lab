# Reviewer code tour

Budget 20 minutes for the core systems evidence and 10 more for supporting
boundaries.

## 1. Deterministic policy (4 minutes)

Open `edge/include/sourcetrace/core/types.hpp`, then
`edge/src/core/reducer.cpp`. Note that event time is data, mode is separate from
health, and transitions return effects without performing I/O.

Review `edge/tests/core_tests.cpp` for safe degraded behavior, physical stop
during storage failure, equal-time ordering, time regression, and byte-for-
behavior trace replay.

## 2. Adversarial byte stream (5 minutes)

Open `edge/include/sourcetrace/protocol/frame.hpp` and
`edge/src/protocol/parser.cpp`. Follow outer-length validation into the fixed
buffer, header and duplicated-length checks, CRC32, signature verification,
bounded JSON, and exact schema.

Review `edge/tests/protocol_tests.cpp`, especially partial/truncated input,
length overflow, invalid flags/enums, CRC/signature distinction, replay
decisions, generated chunking, and the golden vector. The fuzz entry point is
`edge/fuzz/protocol_fuzz.cpp`.

## 3. Atomic acceptance and recovery (6 minutes)

Open `edge/src/runtime/controller_runtime.cpp` and
`edge/src/runtime/sqlite_store.cpp`. Trace how authenticated replay state,
checkpoint, audit, outbound frames, and sequence advance in one transaction.

Review `edge/tests/runtime_tests.cpp` for commit rollback, queue saturation,
restart reconstruction, replay across restart, lost acknowledgement,
five-attempt retry cap, and append-only audit triggers.

## 4. Cross-language executable evidence (5 minutes)

Open `apps/diagnostics/src/simulator-client.ts`,
`apps/diagnostics/src/protocol.ts`, and `apps/diagnostics/src/scenario.ts`.
TypeScript validates the local process boundary and independently inspects
wire bytes; it does not recreate domain policy.

Run `npm run scenario` from `apps/diagnostics`. The output shows full frame hex,
decoded header fields, CRC state, audit tail, queue depth, and receiver effects
through restart and injected failures.

## 5. Document-processing boundary (5 minutes)

Open `services/ingestion/src/sourcetrace_ingestion/processor.py` and
`worker_helper.py`. Review streamed limits, total deadline, helper environment,
fixed subprocess arguments, kill/reap behavior, resource limits, and cleanup.

Then review `tests/test_ingestion.py` for actual-size enforcement, structure and
page limits, idempotency conflict, deadline cleanup, shell-free conversion, and
log redaction.

## 6. Claims and provenance (5 minutes)

Read `docs/security/threat-model.md`, `docs/security/fmea.md`,
`docs/known-limitations.md`, and `docs/ai/verification-matrix.md`.
Session records include meaningful failures and corrections instead of
presenting generated code as trusted by default.

The current placement matters: review this subtree only. The parent `tatiana`
repository is deliberately excluded from the portfolio publication boundary.
