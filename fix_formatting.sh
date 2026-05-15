#!/usr/bin/env bash
# Format src/ with clang-format 14 (matches CI). Uses a local binary when possible;
# falls back to Docker only if clang-format 14 is not on PATH.

set -euo pipefail

CLANG_IMAGE=silkeh/clang:14
REQUIRED_MAJOR=14

usage() {
    echo "Usage: $0 [--fix|--check|--dry-run|--version]" >&2
}

print_install_help() {
    cat >&2 <<'EOF'
clang-format 14 is required but was not found.

Install clang-format 14 for your system, then re-run this script:

  Linux (Ubuntu 22.04)     sudo apt install clang-format
  Linux (Ubuntu 24.04+)    sudo apt install clang-format-14
  macOS (Homebrew)         brew install llvm@14
                           export PATH="$(brew --prefix llvm@14)/bin:$PATH"
  Windows                  .\fix_formatting.ps1  (see script for LLVM / VS install notes)

Optional fallback on Linux/macOS (if you have Docker):
  This script uses silkeh/clang:14 when no local clang-format 14 exists.

EOF
}

version_major() {
    local ver
    ver=$("$1" --version 2>/dev/null | head -1 | grep -oE '[0-9]+' | head -1)
    echo "${ver:-0}"
}

warn_wrong_clang_format_on_path() {
    local candidate major
    for candidate in clang-format clang-format-14; do
        if command -v "$candidate" >/dev/null 2>&1; then
            major=$(version_major "$candidate")
            if [[ "$major" != "0" && "$major" != "$REQUIRED_MAJOR" ]]; then
                echo "warning: $candidate is version $major (need $REQUIRED_MAJOR); ignoring." >&2
            fi
        fi
    done
}

find_local_clang_format() {
    local candidate
    for candidate in clang-format-14 clang-format; do
        if command -v "$candidate" >/dev/null 2>&1; then
            if [[ "$(version_major "$candidate")" == "$REQUIRED_MAJOR" ]]; then
                echo "$candidate"
                return 0
            fi
        fi
    done
    return 1
}

have_docker() {
    command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1
}

ensure_repo_root() {
    if [[ ! -f .clang-format ]] || [[ ! -d src ]]; then
        echo "error: run from the repository root (need .clang-format and src/)." >&2
        exit 1
    fi
}

run_local() {
    local bin=$1
    shift
    local file_count
    file_count=$(find src -type f \( -name '*.cc' -o -name '*.h' \) | wc -l)
    if [[ "$file_count" -eq 0 ]]; then
        echo "error: no .cc/.h files under src/" >&2
        exit 1
    fi
    find src -type f \( -name '*.cc' -o -name '*.h' \) -print0 | xargs -0 "$bin" "$@"
}

run_docker() {
    docker run --rm \
        -v "$(pwd)":/app --workdir /app \
        --user "$(id -u):$(id -g)" \
        "$CLANG_IMAGE" \
        bash -c 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format '"$*"
}

run_format() {
    local bin
    warn_wrong_clang_format_on_path
    if bin=$(find_local_clang_format); then
        run_local "$bin" "$@"
    elif have_docker; then
        echo "note: using Docker ($CLANG_IMAGE). Install clang-format 14 locally to skip Docker." >&2
        run_docker "$@"
    else
        print_install_help
        exit 1
    fi
}

show_version() {
    local bin
    if bin=$(find_local_clang_format); then
        "$bin" --version
    elif have_docker; then
        docker run --rm "$CLANG_IMAGE" clang-format --version
    else
        print_install_help
        exit 1
    fi
}

ensure_repo_root

case "${1:-}" in
    --version)
        show_version
        ;;
    --check)
        run_format --dry-run --Werror
        ;;
    --dry-run)
        run_format --dry-run
        ;;
    '' | --fix)
        run_format -i
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac
