# AI-assisted verification matrix

“Pending” means the work or command has not yet occurred; it is not a pass.

| Change | Human specification | Behavioral tests | Static/security review | Manual validation | Status |
|---|---|---|---|---|---|
| Repository foundation | Task 0001 | CMake configure/build; TypeScript typecheck; 1 test | Dependency audit: 0 vulnerabilities; publication marker scan passed | New repository had 0 inherited objects; foundation script and diff check passed | Reviewed |
| Controller core | Task 0001 | 8 transition/replay/HAL tests passed twice | Warnings as errors; forbidden dependency search clean | Offline, storage-fault, and deterministic replay behavior reviewed | Reviewed |
| Protocol | Task 0001 | 15 protocol tests within 23-test suite; ASan/UBSan 23/23; seeded fuzz 50,000 | Product clang-tidy passed; Ed25519/CRC/replay ordering reviewed | Golden vector matched; parser initialization defect found and fixed | Reviewed |
| Persistence/recovery | Task 0001 | 10 runtime tests within 33-test suite; ASan/UBSan 33/33 | Product clang-tidy passed; atomicity, bounds, retry, replay, and audit semantics reviewed | External CLI restart smoke test matched durable state and queue | Reviewed |
| PDF worker | Task 0001 | 14 pytest cases passed; 81% coverage; final fixture passed local pypdf/Poppler | Ruff and strict mypy passed; subprocess, deadline, resource, log, and idempotency boundaries reviewed | Non-root image built; health and end-to-end tmpfs-backed smoke passed on the text-identical pre-binary-marker fixture revision | Reviewed with noted rerun limit |
| Diagnostic client | Task 0001 | 7 Node tests passed; 15-step real-process scenario passed | Strict TypeScript passed; exact Zod boundaries; cache-only production audit found 0 known vulnerabilities | Shared C++ golden vector matched byte for byte; replay/conflict/CRC/ack-loss/restart behavior reviewed | Reviewed |
| Reviewer evidence | Task 0001 | Pending | Pending | Pending | Pending |
