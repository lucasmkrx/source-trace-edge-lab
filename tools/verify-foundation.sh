#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repository_root"

test "$(git rev-list --all --count)" -le 1
test ! -e .git/shallow
test -f LICENSE
test -f AI_COLLABORATION.md
test -f docs/publication-inventory.md

cmake --preset dev
cmake --build --preset dev

(
  cd apps/diagnostics
  npm run typecheck
  npm test
)

if git grep -Ein \
  'tatiana|firebasestorage|english grammar in use|stormy weather' \
  -- ':!docs/publication-inventory.md'; then
  echo "Forbidden private-project marker found" >&2
  exit 1
fi

echo "Foundation verification passed"
