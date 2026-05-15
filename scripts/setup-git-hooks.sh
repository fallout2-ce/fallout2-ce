#!/usr/bin/env bash
# Point this repository at .githooks/ (pre-commit formats staged C++ files).

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

if [ ! -f .githooks/pre-commit ]; then
    echo "error: missing .githooks/pre-commit" >&2
    exit 1
fi

chmod +x .githooks/pre-commit

git config --local core.hooksPath .githooks

echo "Installed git hooks (core.hooksPath=.githooks)."
echo "pre-commit will format staged src/*.cc and src/*.h when clang-format 14 is available."
