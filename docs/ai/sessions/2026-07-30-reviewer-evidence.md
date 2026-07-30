# AI session: reviewer evidence and verified demonstrations

- Date: 2026-07-30
- Goal: make implemented behavior quickly reviewable without overstating
  verification or publication readiness.
- Human specification: Task 0001, Commit 7 acceptance criteria, amended to keep
  the work inside the original private repository on a new branch.
- AI tool: OpenAI Codex.

## Approach and risks

Codex mapped code and observed tests into a reviewer tour, architecture
overview, threat model, small FMEA, limitations, three demonstration scripts,
verification runbook, and gated publication checklist. It also corrected CI
paths for the lab's nested placement.

The main risk was treating the locally reviewable subtree as evidence that the
entire parent repository was safe to publish. Inspection showed the parent
contains product-specific code, cloud identifiers, migration assets, and
private material. The release recommendation therefore blocks publication of
the parent and requires future extraction and history scanning.

## Suggestions

- Accepted: lead the reviewer from the pure reducer to parser, transaction
  boundary, real-process diagnostics, and PDF isolation in that order.
- Accepted: show exact observed counts and keep the final fixture/container
  rerun caveat visible.
- Modified by human: the original clean sibling repository plan was replaced
  with a branch and subtree in `tatiana`; the standalone checkout was deleted.
- Rejected: no GUI, cloud deployment, remote repository, public release, or
  production integration was added.

## Verification

- `tools/check-publication-scope.sh` and the updated foundation placement check
  passed within the lab subtree.
- `tools/verify-all.sh` passed: C++ 33/33 in 1.39 seconds; TypeScript strict
  typecheck/build and 7/7 tests; Ruff; strict mypy across 10 source files; and
  Python 14/14 in 6.20 seconds with 81% coverage.
- The ASan/UBSan preset rebuilt the complete product and passed 33/33 in 6.24
  seconds without a sanitizer report.
- The clang-tidy preset rebuilt all product targets successfully with warnings
  treated as errors.
- An initial request to build every target in the fuzz preset exposed a local
  Homebrew LLVM 22.1.8/Xcode SDK header-order conflict in SQLite-dependent
  runtime targets. The intended `sourcetrace_protocol_fuzz` target has no
  SQLite dependency, built successfully, and completed 50,000 golden-frame-
  seeded runs in one second with no crash.
- The final ingestion image
  `sha256:d9d45f9f770492a09089ef0cff290b680edca1969b525024364e552172aecbba`
  passed health and exact-final-fixture ingestion as UID 10001, read-only root,
  all capabilities dropped, and bounded tmpfs. The first launch correction for
  tmpfs ownership is recorded in the PDF session.
- The cache-only npm production audit found zero known vulnerabilities. Policy
  denied the live registry audit because dependency-metadata disclosure was not
  separately authorized.
- `git diff --check` passed. No remote, push, deployment, or publication action
  occurred.

See `docs/ai/verification-matrix.md` for the consolidated results.

## Residual risk

Documentation and CI improve reviewability but are not an independent security
assessment. The parent repository remains outside the public artifact boundary.
A future candidate needs clean extraction, history scanning, current dependency
audits, a final-byte container rerun, and second-human review.
