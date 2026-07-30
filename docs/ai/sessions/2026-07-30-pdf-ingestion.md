# AI session: bounded local PDF ingestion

- Date: 2026-07-30
- Goal: implement streamed validation, isolated and resource-limited PDF
  processing, idempotency, redacted logs, behavioral tests, and a local
  container smoke test.
- Human specification: Task 0001, Commit 5 acceptance criteria.
- AI tool: OpenAI Codex.

## Approach and risks

Codex proposed raw streamed PDF requests rather than multipart buffering, an
isolated helper for `pypdf` and Poppler, SQLite request reservations, and a
metadata-only response. Risks included size checks occurring after allocation,
shell injection, stalled uploads, parser resource exhaustion, temp-file leaks,
duplicate work, sensitive logging, and untruthful container claims.

## Suggestions

- Accepted: content and policy hashes in idempotency state, a total monotonic
  deadline, fixed subprocess argument arrays, helper resource limits, an
  allow-listed child environment, and a non-root container.
- Modified: the first asynchronous child-process implementation relied on event
  loop child-watcher behavior. Behavioral tests showed it stalled in FastAPI's
  worker-thread test loop on macOS. It was replaced with explicit `Popen`,
  `shell=False`, `asyncio.to_thread` communication, cancellation kill, and reap.
- Modified: the first Debian container path failed package signature checks.
  Signature verification was not bypassed; the image moved to the official
  Python Alpine variant and signed `apk` Poppler packages.
- Rejected: broad Docker cache/image pruning was not performed when the shared
  Docker disk was full because unrelated user data was outside task scope.

## Verification

- Deterministic fixture generation produced `original-demo.pdf` with SHA-256
  `5c6fe25f89e08fcef2520f8705793e16dfe645b7030e5028bbeddaae8f21bf88`.
- Ruff passed with security rules enabled.
- Strict mypy passed for all 10 source files.
- Pytest passed 14 of 14 behavioral and failure-path tests in 11.73 seconds
  with 81% statement coverage.
- The local image built successfully as
  `sha256:8ae360956261c57a3a526a84d5b005f200e9bb5defed907290f9d645e39165a8`.
- The shared Docker overlay was 100% full, so the first fixture request failed
  with `ENOSPC`. Rather than deleting unrelated cache, the same image was run
  with bounded 16 MiB `tmpfs` mounts for `/tmp` and `/data`.
- The container then passed `/healthz` as non-root user `worker` and processed
  the first generated fixture revision end to end: one page, 113 text bytes,
  content SHA-256
  `f5a8edb0ebb98204d87277f06f3133c844881266cf5468972f1a13361159acec`,
  and text SHA-256
  `3f3eb065c496695905336270c40922aedabffee2ab0a0e2ae94468a2e980a23d`.
- The fixture generator then added a valid NUL to the PDF binary marker so Git
  preserves fixed-width xref rows. The final committed fixture regenerated
  byte-for-byte and passed the full local pypdf/Poppler suite with unchanged
  extracted text. Docker Desktop stopped before that one-byte marker revision
  could be resubmitted, and the environment blocked restarting the GUI because
  its tool-usage limit had been reached.

## Residual risk

Native PDF tooling remains a complex attack surface. Bounds and process
isolation reduce blast radius but are not a sandbox proof. Package scanning,
lease recovery, and read-only container hardening remain publication follow-up
items.
