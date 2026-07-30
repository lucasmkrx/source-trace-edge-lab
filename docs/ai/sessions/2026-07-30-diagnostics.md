# AI session: TypeScript controller diagnostics

- Date: 2026-07-30
- Goal: implement a strict terminal client that drives the compiled C++
  simulator and exposes protocol, persistence, replay, and recovery behavior.
- Human specification: Task 0001, Commit 6 acceptance criteria.
- AI tool: OpenAI Codex.

## Approach and risks

Codex implemented exact Zod contracts on both sides of the local JSON Lines
boundary, a bounded child-process client, an independent v1 binary encoder and
decoder, a terminal renderer, an interactive console, and a deterministic
failure/restart scenario.

Risks included accidentally replacing the C++ domain model with a TypeScript
facsimile, accepting unvalidated process output, shell injection, hanging child
processes, drifting wire implementations, and presenting deterministic test
keys as secrets.

## Suggestions

- Accepted: keep all controller decisions in the C++ process; use TypeScript
  only for validated orchestration, independent frame inspection, and display.
- Accepted: assert one checked-in golden vector byte for byte in both languages.
- Modified: the first scenario assertion assumed the connectivity transition
  would not enqueue telemetry before its injected acknowledgement failure.
  The real runtime correctly committed that transition and fourth frame before
  the flush failed; the assertion was corrected to verify this atomic boundary.
- Modified: direct `tsx` CLI execution attempted to create a local IPC socket
  that the workspace policy disallowed. The scripts now use Node's `--import
  tsx` loader, which needs no IPC server.

## Verification

- Node 22 strict TypeScript type checking passed with no diagnostics.
- The compiled JavaScript build passed.
- Seven Node tests passed, including exact request/response rejection,
  encoder bounds, signed-frame properties, independent CRC validation, the
  shared C++ golden vector, and a real-process integration scenario.
- The 15-step scenario passed and ended with three initially durable records,
  one later offline record, six receiver effects, a drained queue, an accepted
  duplicate, a rejected sequence conflict, and a rejected corrupt frame.
- The existing C++ suite passed 33 of 33 tests after relocation.
- `npm audit --omit=dev --offline` reported zero known vulnerabilities from
  available cache data. A live registry audit was not run because external
  disclosure of dependency metadata was not authorized.

## Residual risk

The local console inherits the simulator's public fixture key and local-user
trust assumptions. It is not a remote administration channel. The independent
TypeScript encoder provides useful drift detection but increases maintenance
work when a future protocol version is introduced.
