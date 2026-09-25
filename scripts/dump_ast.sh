#!/usr/bin/env bash

set -uo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler="$repo_root/build/rx_compiler"
output_dir="$repo_root/build/ast-dumps"
sources=()

usage() {
    cat <<'EOF'
Usage: scripts/dump_ast.sh [--compiler PATH] [--output-dir DIR] [SOURCE.rs ...]

Without SOURCE arguments, dump every .rs file under tests/custom/naive and
tests/custom/normal. Explicit sources must be inside one of those directories.
Dump paths mirror the source paths below tests/custom and use the .ast suffix.
Relative --compiler and --output-dir paths are resolved from the current directory.
EOF
}

while (($#)); do
    case "$1" in
        --compiler)
            if (($# < 2)); then
                printf 'error: --compiler requires a path\n' >&2
                usage >&2
                exit 2
            fi
            compiler=$2
            shift 2
            ;;
        --output-dir)
            if (($# < 2)); then
                printf 'error: --output-dir requires a path\n' >&2
                usage >&2
                exit 2
            fi
            output_dir=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            while (($#)); do
                sources+=("$1")
                shift
            done
            ;;
        -* )
            printf 'error: unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
        *)
            sources+=("$1")
            shift
            ;;
    esac
done

if [[ "$compiler" != /* ]]; then
    compiler="$PWD/$compiler"
fi
if [[ "$output_dir" != /* ]]; then
    output_dir="$PWD/$output_dir"
fi
if [[ ! -x "$compiler" ]]; then
    printf 'error: compiler is missing or not executable: %s\n' "$compiler" >&2
    printf 'Build it first, or pass its location with --compiler.\n' >&2
    exit 1
fi

custom_root="$repo_root/tests/custom"
naive_root="$custom_root/naive"
normal_root="$custom_root/normal"
if ((${#sources[@]} == 0)); then
    list_file=$(mktemp) || {
        printf 'error: cannot create temporary source list\n' >&2
        exit 1
    }
    trap 'rm -f "$list_file"' EXIT
    if ! find "$naive_root" "$normal_root" -type f -name '*.rs' -print | LC_ALL=C sort > "$list_file"; then
        printf 'error: failed to discover custom .rs sources\n' >&2
        exit 1
    fi
    while IFS= read -r source; do
        [[ -n "$source" ]] && sources+=("$source")
    done < "$list_file"
    rm -f "$list_file"
    trap - EXIT
fi

if ((${#sources[@]} == 0)); then
    printf 'error: no .rs sources found\n' >&2
    exit 1
fi

work_dir=$(mktemp -d) || {
    printf 'error: cannot create temporary working directory\n' >&2
    exit 1
}
trap 'rm -rf "$work_dir"' EXIT
failures=0

for source in "${sources[@]}"; do
    if [[ "$source" != /* ]]; then
        source="$PWD/$source"
    fi
    source_dir=$(CDPATH= cd -- "$(dirname -- "$source")" 2>/dev/null && pwd) || source_dir=''
    if [[ -n "$source_dir" ]]; then
        source="$source_dir/$(basename -- "$source")"
    fi
    if [[ ! -f "$source" || "$source" != *.rs ]]; then
        printf 'error: expected an existing .rs file: %s\n' "$source" >&2
        failures=$((failures + 1))
        continue
    fi

    case "$source" in
        "$naive_root"/*|"$normal_root"/*)
            relative_source=${source#"$custom_root"/}
            ;;
        *)
            printf 'error: source must be under tests/custom/naive or tests/custom/normal: %s\n' "$source" >&2
            failures=$((failures + 1))
            continue
            ;;
    esac

    dump_path="$output_dir/${relative_source%.rs}.ast"
    dump_parent=$(dirname -- "$dump_path")
    if ! mkdir -p "$dump_parent"; then
        printf 'error: cannot create output directory: %s\n' "$dump_parent" >&2
        failures=$((failures + 1))
        continue
    fi

    temporary_dump=$(mktemp "$dump_parent/.ast-dump.XXXXXX") || {
        printf 'error: cannot create temporary dump in: %s\n' "$dump_parent" >&2
        failures=$((failures + 1))
        continue
    }
    temporary_error="$work_dir/compiler.stderr"
    if "$compiler" < "$source" > "$temporary_dump" 2> "$temporary_error"; then
        IFS= read -r first_line < "$temporary_dump" || first_line=''
        if [[ "$first_line" == 'Parse Successed.'* ]]; then
            if mv -f "$temporary_dump" "$dump_path"; then
                printf 'wrote %s\n' "$dump_path"
            else
                printf 'error: cannot write dump: %s\n' "$dump_path" >&2
                rm -f "$temporary_dump"
                failures=$((failures + 1))
            fi
        else
            printf 'error: parser did not report success for %s\n' "$source" >&2
            cat "$temporary_error" >&2
            cat "$temporary_dump" >&2
            rm -f "$temporary_dump"
            failures=$((failures + 1))
        fi
    else
        status=$?
        printf 'error: compiler exited with status %d for %s\n' "$status" "$source" >&2
        cat "$temporary_error" >&2
        cat "$temporary_dump" >&2
        rm -f "$temporary_dump"
        failures=$((failures + 1))
    fi
done

if ((failures)); then
    printf 'AST dump generation failed for %d source(s).\n' "$failures" >&2
    exit 1
fi
