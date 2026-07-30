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
| 2026-07-30 | Treat only the lab subtree as a future portfolio candidate and block publication of the enclosing repository. | Parent inspection found product code, cloud configuration, migration assets, and private material outside the subtree. | Accepted security boundary |
| 2026-07-30 | Publish a dependency-free reviewer site that replays checked-in evidence instead of exposing the simulator or ingestion worker. | Human approved a public repository and demo deployment; browser copy explicitly distinguishes recorded evidence from execution. | Accepted |
| 2026-07-30 | Use static Firebase Hosting in the isolated Google Cloud project after Cloud Run billing linkage hit an account quota. | No billing guardrail was bypassed and no unrelated project was reused; the narrower platform matches the static workload. | Verification-driven correction |
| 2026-07-30 | Keep the first public CI failure visible and correct the invalid action pin plus the non-portable CMake SQLite target in a focused follow-up. | GitHub Actions provided independent Linux evidence; macOS C++ and Python jobs already passed. | Verification-driven correction |
| 2026-07-30 | Apply the HTML revalidation policy globally after the first live clean-URL response retained Firebase's one-hour default. | Body integrity and all security headers passed; only cache behavior diverged from the intended configuration. | Live-verification correction |
