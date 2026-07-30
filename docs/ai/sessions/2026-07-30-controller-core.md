# AI session: deterministic controller core

- Date: 2026-07-30
- Goal: implement the pure controller reducer, explicit state/event/effect
  contracts, HAL interfaces, virtual hardware, and deterministic trace replay.
- Human specification: Task 0001, Commit 2 acceptance criteria.
- AI tool: OpenAI Codex.

## Approach and risks

Codex proposed a pure reducer returning state plus requested effects. External
services are represented by narrow interfaces used only by later runtime
coordination. The principal risks were implicit wall-clock access, allowing a
session to start while persistence is unavailable, and treating an offline
session as failed rather than locally degraded.

## Suggestions

- Accepted: separate operating mode from health, retain local session operation
  during network loss, and make event time explicit.
- Modified: storage failure does not forcibly end an already active session; it
  blocks new starts, preserves safe outputs, allows the physical stop, and marks
  resulting audit effects as volatile.
- Rejected: none during this focused task.

## Verification

- `cmake --preset dev`: passed after fetching Catch2 at immutable commit
  `644821ce28cb25d7992a4d0375b1d83214392592`.
- `cmake --build --preset dev`: built the core library and test executable with
  warnings treated as errors.
- `ctest --preset dev`: 8 of 8 behavioral tests passed in 0.23 seconds.
- Repeated `ctest --preset dev --output-on-failure`: 8 of 8 passed in 0.25
  seconds.
- Architecture search for filesystem, SQLite, sockets, cloud SDKs, and wall-clock
  APIs in the domain core returned no matches.
- `git diff --check`: passed.

## Residual risk

Effects are declarative in this commit. Atomic persistence, authenticated
commands, bounded queueing, and runtime effect execution are implemented and
tested in later focused commits.
