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

format_docker_user_args() {
    case "$(uname -s)" in
        Linux | Darwin) printf '%s' "--user $(id -u):$(id -g)" ;;
    esac
}

format_warn_wrong_clang_format_on_path() {
    for candidate in clang-format clang-format-14; do
        if command -v "$candidate" >/dev/null 2>&1; then
            local major
            major=$(format_version_major "$candidate")
            if [ "$major" != "0" ] && [ "$major" != "$FORMATTER_REQUIRED_MAJOR" ]; then
                echo "warning: $candidate is version $major (need $FORMATTER_REQUIRED_MAJOR); ignoring." >&2
            fi
        fi
    done
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
        local root user_args
        root=$(pwd)
        user_args=$(format_docker_user_args)
        # shellcheck disable=SC2086
        docker run --rm $user_args \
            -v "$root":/app --workdir /app \
            "$FORMATTER_CLANG_IMAGE" \
            clang-format -i "$@"
        return 0
    fi

    return 1
}

# Format all of src/ with clang-format flags (e.g. -i or --dry-run --Werror). Returns 0/1.
format_run_src() {
    local bin user_args
    format_warn_wrong_clang_format_on_path

    if bin=$(format_find_clang_format); then
        find src -type f \( -name '*.cc' -o -name '*.h' \) -print0 |
            xargs -0 "$bin" "$@"
        return 0
    fi

    if format_have_docker; then
        echo "note: using Docker ($FORMATTER_CLANG_IMAGE). Install clang-format 14 locally to skip Docker." >&2
        user_args=$(format_docker_user_args)
        # shellcheck disable=SC2086
        docker run --rm $user_args \
            -v "$(pwd)":/app --workdir /app \
            "$FORMATTER_CLANG_IMAGE" \
            bash -c 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 clang-format '"$*"
        return 0
    fi

    return 1
}

format_show_version() {
    local bin
    if bin=$(format_find_clang_format); then
        "$bin" --version
        return 0
    fi
    if format_have_docker; then
        docker run --rm "$FORMATTER_CLANG_IMAGE" clang-format --version
        return 0
    fi
    return 1
}

format_print_install_help() {
    cat >&2 <<'EOF'
clang-format 14 is required but was not found.

Install clang-format 14 for your system, then re-run:

  Linux (Ubuntu 22.04)     sudo apt install clang-format
  Linux (Ubuntu 24.04+)    sudo apt install clang-format-14
  macOS (Homebrew)         brew install llvm@14
                           export PATH="$(brew --prefix llvm@14)/bin:$PATH"
  Windows                  .\fix_formatting.ps1

Optional fallback (Docker): uses silkeh/clang:14 when clang-format 14 is not on PATH.
EOF
}

format_print_hook_warning() {
    cat <<'EOF' >&2
pre-commit: clang-format 14 not found — skipping format of staged C++ files.
Install clang-format 14 (see CONTRIBUTING.md) or use Docker; see ./fix_formatting.sh
EOF
}
