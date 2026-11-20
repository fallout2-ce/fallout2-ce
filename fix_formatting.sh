#!/bin/bash
set -euo pipefail

# Use the same formatter for local fixes, CI checks, and automatic commits.
case "${1:---fix}" in
  --fix) format_args=(-i) ;;
  --check) format_args=(--dry-run --Werror) ;;
  --dry-run) format_args=(--dry-run) ;;
  --version) format_args=(--version) ;;
  --help|-h)
    echo "Usage: $0 [--fix|--check|--dry-run|--version]"
    echo "Requires Docker; uses clang-format 18 (silkeh/clang:18)."
    exit 0
    ;;
  *) echo "Unknown option: $1" >&2; exit 2 ;;
esac

if (( $# > 1 )); then
  echo "Expected at most one option" >&2
  exit 2
fi

cd "$(dirname "$0")"
docker_args=(--rm -v "$PWD:/app" --workdir /app --user "$(id -u):$(id -g)" silkeh/clang:18)
if [[ "${format_args[0]}" == --version ]]; then
  exec docker run "${docker_args[@]}" clang-format --version
fi

exec docker run "${docker_args[@]}" \
  bash -c 'find src -type f \( -name "*.cc" -o -name "*.h" \) -exec clang-format "$@" {} +' bash "${format_args[@]}"

