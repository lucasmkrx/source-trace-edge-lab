# AI-assisted decision log

| Date | Decision | Human review | Status |
|---|---|---|---|
| 2026-07-30 | Keep the private product unchanged and create a clean sibling repository. | Approved explicitly. | Accepted |
| 2026-07-30 | Reduce the initial AI proposal from a broad 28–45 day platform to a 7–12 day C++-centered MVP. | Human rejected scope expansion and deferred React, Firebase, Electron, Vue, GTKmm, and cloud deployment. | Human correction |
| 2026-07-30 | Use a pure reducer plus adapter-driven runtime rather than injecting services into domain transitions. | Approved in the implementation plan. | Accepted |
| 2026-07-30 | Use Ed25519 for authenticated commands and CRC32 only for accidental corruption. | Approved in the implementation plan. | Accepted |
| 2026-07-30 | Use one-writer SQLite rollback-journal persistence and describe retransmission plus idempotency instead of exactly-once delivery. | Approved in the implementation plan. | Accepted |
| 2026-07-30 | Persist a five-attempt retry budget with explicit backoff per connection and reset it only on a connectivity-restored event. | Checked against the approved bounded-retry requirement and behavioral tests. | Accepted |
| 2026-07-30 | Isolate PDF parsing and `pdftotext` in a resource-limited helper and return metadata rather than extracted document text. | Checked against the approved minimal local worker and privacy boundary. | Accepted |
| 2026-07-30 | Replace the macOS worker-thread `asyncio` subprocess path after deadline tests exposed child-watcher stalling. | Behavioral failure drove a reviewed `Popen`/kill/reap correction. | Human/verification correction |
| 2026-07-30 | Use a strict TypeScript terminal client rather than a GUI in the MVP. | Human explicitly deferred Electron/Vue. | Accepted |
| 2026-07-30 | Keep controller decisions in C++ and make TypeScript a validated diagnostic orchestrator with an independent wire inspector. | Confirmed with shared golden-vector and real-process tests. | Accepted |
| 2026-07-30 | Import the completed lab history as a subtree of the original `tatiana` repository and delete the temporary standalone checkout. | Human explicitly replaced the sibling-repository instruction before Commit 6. | Human correction |
