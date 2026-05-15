#!/bin/bash

set -euo pipefail

CLANG_IMAGE=silkeh/clang:14

run_clang_format() {
    docker run --rm \
        -v "$(pwd)":/app --workdir /app \
        --user "$(id -u):$(id -g)" \
        "$CLANG_IMAGE" \
        bash -c 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format '"$*"
}

case "${1:-}" in
    --version)
        docker run --rm "$CLANG_IMAGE" clang-format --version
        ;;
    --check)
        run_clang_format --dry-run --Werror
        ;;
    --dry-run)
        run_clang_format --dry-run
        ;;
    "" | --fix)
        run_clang_format -i
        ;;
    *)
        echo "Usage: $0 [--fix|--check|--dry-run|--version]" >&2
        exit 1
        ;;
esac
