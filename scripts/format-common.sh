# Shared clang-format 14 helpers (sourced by fix_formatting.sh and .githooks/pre-commit).
# Do not execute directly.

FORMATTER_REQUIRED_MAJOR=14
FORMATTER_CLANG_IMAGE=silkeh/clang:14

format_version_major() {
    "$1" --version 2>/dev/null | head -1 | grep -oE '[0-9]+' | head -1
}

format_find_clang_format() {
    for candidate in clang-format-14 clang-format; do
        if command -v "$candidate" >/dev/null 2>&1; then
            if [ "$(format_version_major "$candidate")" = "$FORMATTER_REQUIRED_MAJOR" ]; then
                echo "$candidate"
                return 0
            fi
        fi
    done
    return 1
}

format_have_docker() {
    command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1
}

# Format the given file paths (repo-relative). Returns 0 on success, 1 if no formatter.
format_files_inplace() {
    if [ "$#" -eq 0 ]; then
        return 0
    fi

    local bin
    if bin=$(format_find_clang_format); then
        "$bin" -i "$@"
        return 0
    fi

    if format_have_docker; then
        local root
        root=$(pwd)
        docker run --rm \
            -v "$root":/app --workdir /app \
            --user "$(id -u):$(id -g)" 2>/dev/null \
            "$FORMATTER_CLANG_IMAGE" \
            clang-format -i "$@"
        return 0
    fi

    return 1
}

format_print_hook_warning() {
    cat <<'EOF' >&2
pre-commit: clang-format 14 not found — skipping format of staged C++ files.
Install clang-format 14 (see CONTRIBUTING.md) to enable format-on-commit.
See CONTRIBUTING.md (Code formatting) or use ./fix_formatting.sh / .\fix_formatting.ps1
EOF
}
