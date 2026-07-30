#!/usr/bin/env bash
set -euo pipefail

lab_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$lab_root"

test -f LICENSE
test -f NOTICE
test -f SECURITY.md
test -f AI_COLLABORATION.md
test -f docs/publication-inventory.md
test -f docs/runbooks/publication-checklist.md

if git grep -nEi \
  'firebasestorage|english grammar in use|stormy weather|AIza[0-9A-Za-z_-]{30,}|-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----' \
  -- . \
  ':(exclude)docs/publication-inventory.md' \
  ':(exclude)docs/ai/decision-log.md' \
  ':(exclude)docs/ai/sessions/2026-07-30-diagnostics.md' \
  ':(exclude)docs/ai/sessions/2026-07-30-reviewer-evidence.md' \
  ':(exclude)tools/verify-foundation.sh' \
  ':(exclude)tools/check-publication-scope.sh'; then
  echo "Excluded parent-project marker or private-key material found in lab scope" >&2
  exit 1
fi

fixture="services/ingestion/tests/fixtures/original-demo.pdf"
test "$(git ls-files -- "$fixture")" = "$fixture"
test "$(find . -type f \( -name '*.pdf' -o -name '*.png' -o -name '*.jpg' -o -name '*.webp' \) \
  -not -path './build/*' -not -path './**/.venv/*' | wc -l | tr -d ' ')" -eq 1

echo "Lab publication-scope structural scan passed"
