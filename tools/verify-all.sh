#!/usr/bin/env bash
set -euo pipefail

lab_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$lab_root"

./tools/check-publication-scope.sh

cmake --preset dev
cmake --build --preset dev
ctest --preset dev

(
  cd apps/diagnostics
  npm run typecheck
  npm run build
  npm test
)

python_runner="python3"
if [[ -x services/ingestion/.venv/bin/python ]]; then
  python_runner="$lab_root/services/ingestion/.venv/bin/python"
fi
(
  cd services/ingestion
  "$python_runner" -m ruff check .
  "$python_runner" -m mypy src
  "$python_runner" -m pytest
)

git diff --check -- .
echo "Complete local verification passed"
