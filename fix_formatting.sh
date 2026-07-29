#!/usr/bin/env bash
# Format src/ with clang-format 14 (matches CI). Uses a local binary when possible;
# falls back to Docker if clang-format 14 is not on PATH (Linux, macOS, Windows).

set -euo pipefail

ROOT=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=scripts/format-common.sh
. "$ROOT/scripts/format-common.sh"

usage() {
    echo "Usage: $0 [--fix|--check|--dry-run|--version]" >&2
}

ensure_repo_root() {
    if [[ ! -f .clang-format ]] || [[ ! -d src ]]; then
        echo "error: run from the repository root (need .clang-format and src/)." >&2
        exit 1
    fi
}

run_format() {
    local rc=0
    format_run_src "$@" || rc=$?
    if [ "$rc" -eq 0 ]; then
        return 0
    fi
    if [ "$rc" -eq 1 ]; then
        format_print_install_help
    fi
    exit "$rc"
}

ensure_repo_root

case "${1:-}" in
    --version)
        if ! format_show_version; then
            format_print_install_help
            exit 1
        fi
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
