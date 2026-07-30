# Small failure-mode and effects analysis

Scores use 1 (low) through 5 (high). They are prioritization aids, not
certification evidence.

| Failure mode | Effect | Severity | Likelihood | Detection | Control and tested response | Residual |
|---|---|---:|---:|---:|---|---|
| Corrupt or truncated frame | Incorrect parse or stalled stream | 4 | 3 | 1 | Early fixed-buffer length, CRC, and partial-frame tests; reject frame | Reconnect policy remains transport-specific |
| Forged command | Unauthorized controller transition | 5 | 2 | 2 | Ed25519 before JSON/application; corrupted signature tests | Key lifecycle absent |
| Replay/conflicting sequence | Repeated or ambiguous effect | 4 | 3 | 1 | Durable sequence/signature decision; restart tests | One trusted signer model |
| SQLite commit failure | State and replay state diverge | 5 | 2 | 1 | Atomic batch rollback and storage-fault state; injected failure tests | Hardware/filesystem guarantees vary |
| Queue capacity reached | Telemetry loss or unsafe new work | 4 | 3 | 1 | Explicit record/byte cap; durable saturation; block new session | Operator intervention required |
| Ack deletion fails | Duplicate remote application | 3 | 3 | 1 | Stable event ID and idempotent receiver; lost-ack test | Real receiver must implement same contract |
| Network stays unavailable | Queue growth and stale remote view | 3 | 4 | 1 | Local session continues degraded; capped queue/retries | No remote availability guarantee |
| Process restarts | Lost state or retry budget | 4 | 3 | 1 | Durable checkpoint/replay/queue/retry reconstruction tests | Power-loss durability is host-dependent |
| PDF parser stalls/exhausts | Worker denial of service | 4 | 3 | 2 | Total deadline, helper kill/reap, resource and size limits | Kernel limits vary; parser vulnerabilities remain |
| Temp cleanup fails after crash | Sensitive bytes remain locally | 4 | 2 | 3 | Private temp directory and normal/cancel cleanup tests | Abrupt host loss can leave files |
| Diagnostic process lies/hangs | Reviewer acts on invalid output | 2 | 2 | 1 | Exact response schema, timeout, terminate on violation | Local executable replacement is out of scope |
