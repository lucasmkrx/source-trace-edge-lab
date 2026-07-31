# SourceTrace Edge Lab

[![Continuous Integration](https://github.com/lucasmkrx/source-trace-edge-lab/actions/workflows/ci.yml/badge.svg)](https://github.com/lucasmkrx/source-trace-edge-lab/actions/workflows/ci.yml)
[![Release](https://github.com/lucasmkrx/source-trace-edge-lab/actions/workflows/release.yml/badge.svg)](https://github.com/lucasmkrx/source-trace-edge-lab/actions/workflows/release.yml)
[![codecov](https://codecov.io/gh/lucasmkrx/source-trace-edge-lab/graph/badge.svg)](https://codecov.io/gh/lucasmkrx/source-trace-edge-lab)

SourceTrace Edge Lab is a local, C++20-centered systems portfolio prototype for
a deterministic classroom edge terminal. It demonstrates bounded binary
parsing, authenticated commands, replay protection, transactional offline
recovery, fault injection, a hardened local PDF worker, and a strict
TypeScript diagnostic console.

It is executable engineering evidence, not a production device, safety system,
election system, or certified security product.

The dependency-free reviewer showcase under `showcase/` presents the verified
architecture, 15-step recovery trace, normative frame decoder, and explicit
human/AI decision history without exposing any runtime service.

The public deployment is intentionally static. Its configuration deploys only
`showcase/public/`; the controller, worker, database, fixtures, and credentials
remain source or local test artifacts.

## Verified snapshot

The implementation was verified locally on 2026-07-30:

| Evidence | Observed result |
|---|---|
| C++ developer suite | 33/33 CTest cases passed |
| C++ ASan + UBSan suite | 33/33 passed without a sanitizer report |
| Static analysis | product clang-tidy build passed with warnings as errors |
| Parser fuzz smoke | 50,000 seeded runs completed without a crash |
| Python worker | 14/14 pytest cases, 81% statement coverage, Ruff and strict mypy passed |
| Python container | final fixture passed in image `sha256:d9d45f9f…` as non-root with read-only root, no capabilities, and bounded tmpfs |
| TypeScript diagnostics | strict typecheck/build and 7/7 tests passed |
| Cross-language scenario | 15 steps passed against the real C++ process |

The committed final PDF fixture passed local `pypdf`/Poppler tests and the final
container smoke. Its content SHA-256 is
`5c6fe25f89e08fcef2520f8705793e16dfe645b7030e5028bbeddaae8f21bf88`.

## Design

- A pure reducer owns deterministic controller transitions and has no
  filesystem, socket, database, cloud, UI, or wall-clock dependency.
- A runtime adapter commits controller checkpoints, audit entries, outbound
  frames, next sequence, and authenticated replay state in one SQLite
  transaction.
- A fixed-buffer stream parser validates outer length before accepting a body,
  then validates header, inner lengths, CRC32, Ed25519, JSON bounds, and exact
  message schema.
- Delivery is at least once. Stable event identifiers make receiver effects
  idempotent when acknowledgement loss causes retransmission.
- The Python worker streams bounded input, isolates parsing/conversion in a
  helper process, applies deadlines and resource limits where supported,
  retains metadata only, and redacts logs.
- The TypeScript console validates every local control message and independently
  decodes the actual binary frames emitted by C++.

Start with [the reviewer tour](docs/reviewer-tour.md), then use the
[architecture overview](docs/architecture/overview.md), [protocol v1
specification](docs/protocol/v1.md), [threat model](docs/security/threat-model.md),
and [recovery model](docs/architecture/recovery.md).

## Quick start

Prerequisites:

- CMake 3.25+, Ninja, a C++20 compiler, libsodium, and SQLite
- Node.js 22 and npm
- Python 3.12 or 3.13, Poppler (`pdftotext`), and a virtual environment

Ubuntu packages:

```sh
sudo apt-get update
sudo apt-get install --yes cmake ninja-build libsodium-dev libsqlite3-dev \
  python3-venv poppler-utils
```

macOS packages:

```sh
brew install cmake ninja libsodium sqlite node@22 python@3.13 poppler llvm
```

Build the controller and run its tests:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the diagnostic integration scenario:

```sh
cd apps/diagnostics
npm ci
npm test
npm run scenario
```

Run the worker checks:

```sh
cd services/ingestion
python3.13 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.lock
.venv/bin/python -m ruff check .
.venv/bin/python -m mypy src
.venv/bin/python -m pytest
```

The complete repeatable command list and optional deep checks are in
[the local verification runbook](docs/runbooks/local-verification.md). Static
hosting and rollback are documented in [the deployment
runbook](docs/runbooks/deployment.md).

## Three reviewer demonstrations

1. [Deterministic safe-state transitions](docs/demos/01-deterministic-controller.md)
2. [Bounded protocol abuse and authentication](docs/demos/02-protocol-abuse.md)
3. [Offline recovery, idempotency, and document isolation](docs/demos/03-recovery-and-ingestion.md)

Each script states the claim, exact commands, expected evidence, and what must
not be claimed.

## Repository map

- `edge/include/sourcetrace/core` and `edge/src/core`: pure domain contracts and
  reducer.
- `edge/include/sourcetrace/protocol` and `edge/src/protocol`: bounded framing,
  validation, CRC32, signatures, and replay decisions.
- `edge/src/runtime`: transactional coordination and SQLite recovery.
- `edge/simulator`, `edge/cli`, `edge/tests`, `edge/fuzz`: deterministic
  hardware/transport, process interface, behavioral tests, and fuzz target.
- `services/ingestion`: local metadata-only PDF ingestion worker.
- `apps/diagnostics`: strict TypeScript console and real-process scenario.
- `showcase`: static reviewer experience and browser/model contract tests.
- `docs`: architecture, security, protocol, runbooks, demos, decisions, and
  observed AI-assisted verification.

## Security and release status

CRC32 detects accidental corruption; Ed25519 authenticates configuration and
command fixtures. The fixed demo key is public test data. The simulator
transport is local and unencrypted. Local administrators, executable
replacement, database replacement, key lifecycle, secure boot, hardware
attestation, and a production-grade hostile-document sandbox are outside scope.
Read [SECURITY.md](SECURITY.md) and [known limitations](docs/known-limitations.md).

The public history is extracted from the approved lab subtree only. The
enclosing private product repository remains outside the security and
publication boundary. Release work must follow the [publication
checklist](docs/runbooks/publication-checklist.md) and [deployment
runbook](docs/runbooks/deployment.md).

## License

The lab subtree is licensed under Apache License 2.0. See [LICENSE](LICENSE),
[NOTICE](NOTICE), and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
