#!/usr/bin/env bash
set -u
set -o pipefail

suite_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$suite_dir/../../.." && pwd)"
output_dir="$repo_dir/build/custom-ir"
cxx="${CXX:-c++}"

if [[ -n "${LLVM_BIN:-}" ]]; then
    llvm_as="$LLVM_BIN/llvm-as"
    llvm_opt="$LLVM_BIN/opt"
    filecheck="$LLVM_BIN/FileCheck"
else
    llvm_as="llvm-as"
    llvm_opt="opt"
    filecheck="FileCheck"
fi
for tool in "$cxx" "$llvm_as" "$llvm_opt" "$filecheck"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing tool: $tool (set CXX or LLVM_BIN)." >&2
        exit 2
    fi
done
for tool in "$llvm_as" "$llvm_opt"; do
    if ! "$tool" --version | grep -Eq 'LLVM version 22\.'; then
        echo "LLVM 22 is required: $tool" >&2
        exit 2
    fi
done

mkdir -p "$output_dir" || exit 2
flags=(-std=c++17 -Wall -Wextra -Wpedantic -I"$repo_dir/src")
objects=()
: > "$output_dir/build-ir.log"
for implementation in types builder printer; do
    object="$output_dir/$implementation.o"
    rm -f "$object"
    if ! "$cxx" "${flags[@]}" -c "$repo_dir/src/ir/$implementation.cpp" \
        -o "$object" >> "$output_dir/build-ir.log" 2>&1; then
        echo "FAIL shared IR compilation; see $output_dir/build-ir.log" >&2
        exit 1
    fi
    objects+=("$object")
done

shopt -s nullglob
cases=("$suite_dir"/[0-9][0-9]_*.cpp)
if [[ ${#cases[@]} -ne 10 ]]; then
    echo "Expected 10 IR programs, found ${#cases[@]}." >&2
    exit 2
fi
passed=0
failed=0
summary="$output_dir/summary.txt"
: > "$summary"
for source in "${cases[@]}"; do
    name="$(basename -- "$source" .cpp)"
    prefix="$output_dir/$name"
    # Prevent a failed build or parse from reusing an earlier run's artifacts.
    rm -f "$prefix" "$prefix.ll" "$prefix.bc" "$prefix.build.log" \
        "$prefix.generate.log" "$prefix.filecheck.log" "$prefix.llvm-as.log" "$prefix.verify.log"
    if ! "$cxx" "${flags[@]}" "$source" "${objects[@]}" -o "$prefix" \
        > "$prefix.build.log" 2>&1; then
        echo "FAIL $name [C++ compilation]" | tee -a "$summary"
        failed=$((failed + 1))
        continue
    fi
    if ! "$prefix" > "$prefix.ll" 2> "$prefix.generate.log"; then
        echo "FAIL $name [IR generation]" | tee -a "$summary"
        failed=$((failed + 1))
        continue
    fi
    errors=()
    if ! "$filecheck" "$source" --enable-var-scope --input-file="$prefix.ll" \
        > "$prefix.filecheck.log" 2>&1; then
        errors+=(FileCheck)
    fi
    if "$llvm_as" "$prefix.ll" -o "$prefix.bc" > "$prefix.llvm-as.log" 2>&1; then
        if ! "$llvm_opt" -passes=verify -disable-output "$prefix.bc" \
            > "$prefix.verify.log" 2>&1; then
            errors+=(verifier)
        fi
    else
        errors+=(llvm-as)
        echo "SKIPPED: llvm-as failed; no bitcode to verify." > "$prefix.verify.log"
        rm -f "$prefix.bc"
    fi
    if [[ ${#errors[@]} -eq 0 ]]; then
        echo "PASS $name [generation, FileCheck, llvm-as, verifier]" | tee -a "$summary"
        passed=$((passed + 1))
    else
        echo "FAIL $name [${errors[*]}]" | tee -a "$summary"
        failed=$((failed + 1))
    fi
done
echo "Result: $passed passed, $failed failed. Artifacts: $output_dir" | tee -a "$summary"
[[ $failed -eq 0 ]]
