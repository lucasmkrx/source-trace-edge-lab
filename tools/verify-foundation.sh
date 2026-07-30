#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

test -f "$repository_root/LICENSE"
test -f "$repository_root/AI_COLLABORATION.md"
test -f "$repository_root/docs/publication-inventory.md"

"$repository_root/tools/check-publication-scope.sh"
echo "Foundation and current placement checks passed"
