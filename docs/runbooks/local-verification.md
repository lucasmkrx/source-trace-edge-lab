# Local verification runbook

Run commands from the lab directory, whether it is nested at
`tatiana/source-trace-edge-lab` or later extracted.

## Fast reviewer path

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

cd apps/diagnostics
npm ci
npm run typecheck
npm run build
npm test
npm run scenario
```

The diagnostics tests require the developer C++ build because their integration
case spawns `build/dev/edge/sourcetrace_sim`.

## Python worker

```sh
cd services/ingestion
python3.13 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.lock
.venv/bin/python -m ruff check .
.venv/bin/python -m mypy src
.venv/bin/python -m pytest
```

`pdftotext` must be installed and visible on `PATH`.

## Sanitizers

Use Clang or GCC on Linux, or AppleClang on macOS:

```sh
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan
```

## Static analysis

Place a recent `clang-tidy` on `PATH`:

```sh
cmake --preset clang-tidy
cmake --build --preset clang-tidy
```

Only product targets receive clang-tidy; vendored Catch2 is excluded.

## Fuzz smoke

libFuzzer requires a Clang distribution containing the fuzzer runtime:

```sh
CC=clang CXX=clang++ cmake --preset fuzz
cmake --build --preset fuzz --target sourcetrace_protocol_fuzz
mkdir -p build/fuzz-corpus
python3 - <<'PY'
import json
from pathlib import Path
value = json.loads(Path("docs/protocol/golden-vectors.json").read_text())
Path("build/fuzz-corpus/golden-frame").write_bytes(
    bytes.fromhex(value["unsignedTelemetry"]["frameHex"])
)
PY
build/fuzz/edge/sourcetrace_protocol_fuzz \
  -runs=50000 -max_len=4212 build/fuzz-corpus
```

The checked-in historical result used Homebrew LLVM 22.1.8 because the Xcode
toolchain lacked the libFuzzer runtime.

## Container

Docker must be running:

```sh
docker build -t sourcetrace-ingestion:local services/ingestion
docker run --rm --read-only --user 10001:10001 \
  --cap-drop ALL --security-opt no-new-privileges \
  --tmpfs /tmp:rw,noexec,nosuid,size=16m,mode=1777 \
  --tmpfs /data:rw,noexec,nosuid,size=16m,mode=1777 \
  -p 127.0.0.1:8080:8080 sourcetrace-ingestion:local
```

Use `/healthz` and then post the generated fixture with the headers documented
in `docs/security/pdf-ingestion.md`. Do not place private documents in a smoke
test.

## Evidence rules

- Record toolchain versions and exact observed counts.
- Treat a skipped command as unverified.
- Do not translate CRC success into sender authenticity.
- Do not translate idempotent receiver behavior into exactly-once transport.
- Do not claim the parent repository passed the lab's publication scan.
