#!/usr/bin/env bash
set -u
set -o pipefail

suite_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$suite_dir/../../.." && pwd)"
mode="${MODE:-debug}"
filter="${FILTER:-*}"
output_dir="$repo_dir/build/custom-ir/$mode"
cxx="${CXX:-c++}"
flags=(-std=c++17 -Wall -Wextra -Wpedantic -I"$repo_dir/src")
case "$mode" in
    debug) flags+=(-O0 -g) ;;
    release) flags+=(-O2 -DNDEBUG) ;;
    sanitize) flags+=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer) ;;
    coverage) flags+=(-O0 -g -fprofile-instr-generate -fcoverage-mapping) ;;
    *) echo "Unknown MODE: $mode (debug|release|sanitize|coverage)" >&2; exit 2 ;;
esac
llvm_tool() {
    if [[ -n "${LLVM_BIN:-}" ]]; then printf '%s/%s' "$LLVM_BIN" "$1"; else printf '%s' "$1"; fi
}
llvm_as="$(llvm_tool llvm-as)"
llvm_opt="$(llvm_tool opt)"
filecheck="$(llvm_tool FileCheck)"
tools=("$cxx" "$llvm_as" "$llvm_opt" "$filecheck")
if [[ "$mode" == coverage ]]; then
    llvm_cov="$(llvm_tool llvm-cov)"
    llvm_profdata="$(llvm_tool llvm-profdata)"
    tools+=("$llvm_cov" "$llvm_profdata")
fi
for tool in "${tools[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing tool: $tool (set CXX or LLVM_BIN)." >&2; exit 2
    fi
done
for tool in "${tools[@]:1}"; do
    if ! "$tool" --version | grep -Eq 'LLVM version 22\.'; then
        echo "LLVM 22 is required: $tool" >&2; exit 2
    fi
done
if [[ "$mode" == coverage ]] && ! "$cxx" --version | grep -Eqi 'clang'; then
    echo "coverage mode requires Clang (set CXX)." >&2; exit 2
fi
mkdir -p "$output_dir" || exit 2
if [[ "$mode" == coverage ]]; then
    mkdir -p "$output_dir/profiles" || exit 2
    rm -f "$output_dir/profiles/"*.profraw "$output_dir/coverage.profdata" "$output_dir/coverage.txt" "$output_dir/coverage.json"
    # Only this mode's generated HTML report is disposable.
    rm -rf "$output_dir/coverage-html"
    export LLVM_PROFILE_FILE="$output_dir/profiles/%m-%p.profraw"
fi
objects=()
: > "$output_dir/build-ir.log"
for implementation in types builder printer verifier; do
    object="$output_dir/$implementation.o"
    rm -f "$object"
    if ! "$cxx" "${flags[@]}" -c "$repo_dir/src/ir/$implementation.cpp" -o "$object" >> "$output_dir/build-ir.log" 2>&1; then
        echo "FAIL shared IR compilation; see $output_dir/build-ir.log" >&2; exit 1
    fi
    objects+=("$object")
done
shopt -s nullglob
unit_sources=("$suite_dir"/unit/*.cpp)
programs=("$suite_dir"/[0-9][0-9]_*.cpp)
if [[ ${#unit_sources[@]} -eq 0 || ${#programs[@]} -eq 0 ]]; then
    echo "No unit/integration sources found." >&2; exit 2
fi
runner="$output_dir/ir-unit"
rm -f "$runner" "$output_dir/cases.tsv"
if ! "$cxx" "${flags[@]}" "${unit_sources[@]}" "${objects[@]}" -o "$runner" > "$output_dir/unit-build.log" 2>&1; then
    echo "FAIL unit compilation; see $output_dir/unit-build.log" >&2; exit 1
fi
if ! "$runner" --list > "$output_dir/cases.tsv" 2> "$output_dir/list.log"; then
    echo "Cannot enumerate unit tests; see $output_dir/list.log" >&2; exit 2
fi
passed=0
failed=0
selected=0
infrastructure_error=0
summary="$output_dir/summary.txt"
results="$output_dir/results.tsv"
: > "$summary"
printf 'case\tstatus\tdetail\n' > "$results"
record() {
    local name="$1" status="$2" detail="$3"
    printf '%s %s [%s]\n' "$status" "$name" "$detail" >> "$summary"
    printf '%s\t%s\t%s\n' "$name" "$status" "$detail" >> "$results"
    if [[ "$status" == PASS ]]; then passed=$((passed + 1)); else failed=$((failed + 1)); fi
    if [[ "$status" != PASS || "$name" == [0-9][0-9]_* ]]; then
        printf '%s %s [%s]\n' "$status" "$name" "$detail"
    fi
}
# Sets stage_errors; expected rejection must exit 1 and diagnose the intended rule.
llvm_check() {
    local directory="$1" input="$2" oracle="$3" diagnostic="$4" parse_rc verify_rc
    if [[ "$oracle" == none ]]; then return; fi
    if [[ ! -f "$input" ]]; then stage_errors+=(missing-IR); return; fi
    "$llvm_as" -disable-verify "$input" -o "$directory/module.bc" > "$directory/llvm-as.log" 2>&1
    parse_rc=$?
    if [[ "$oracle" == reject-parse ]]; then
        if [[ $parse_rc -ne 1 ]] || ! grep -Fq -- "$diagnostic" "$directory/llvm-as.log"; then
            stage_errors+=(LLVM-parse-rejection)
        fi
        rm -f "$directory/module.bc"
        echo 'NOT RUN: test expects rejection during parsing.' > "$directory/llvm-verify.log"
        return
    fi
    if [[ $parse_rc -ne 0 ]]; then
        stage_errors+=(LLVM-parse)
        rm -f "$directory/module.bc"
        echo 'NOT RUN: parsing failed.' > "$directory/llvm-verify.log"
        return
    fi
    "$llvm_opt" -passes=verify -disable-output "$directory/module.bc" > "$directory/llvm-verify.log" 2>&1
    verify_rc=$?
    if [[ "$oracle" == reject-verify ]]; then
        if [[ $verify_rc -ne 1 ]] || ! grep -Fq -- "$diagnostic" "$directory/llvm-verify.log"; then
            stage_errors+=(LLVM-verifier-rejection)
        fi
    elif [[ $verify_rc -ne 0 ]]; then
        stage_errors+=(LLVM-verifier)
    fi
}
binaries=("$runner")
for source in "${programs[@]}"; do
    name="$(basename -- "$source" .cpp)"
    [[ "$name" == $filter ]] || continue
    selected=$((selected + 1))
    directory="$output_dir/$name"
    mkdir -p "$directory" || exit 2
    rm -f "$directory/"*.ll "$directory/"*.bc "$directory/"*.log "$directory/program"
    if ! "$cxx" "${flags[@]}" "$source" "${objects[@]}" -o "$directory/program" > "$directory/build.log" 2>&1; then
        record "$name" FAIL 'C++ compilation'; continue
    fi
    binaries+=("$directory/program")
    if ! "$directory/program" > "$directory/module.ll" 2> "$directory/project-verifier.log"; then
        record "$name" FAIL 'generation/project-verifier'; continue
    fi
    stage_errors=()
    if ! "$filecheck" "$source" --enable-var-scope --input-file="$directory/module.ll" > "$directory/filecheck.log" 2>&1; then
        stage_errors+=(FileCheck)
    fi
    llvm_check "$directory" "$directory/module.ll" accept ''
    if [[ ${#stage_errors[@]} -eq 0 ]]; then
        record "$name" PASS 'project-verifier, FileCheck, LLVM-parse, LLVM-verifier'
    else record "$name" FAIL "${stage_errors[*]}"; fi
done
while IFS=$'\t' read -r name oracle diagnostic; do
    [[ "$name" == $filter ]] || continue
    selected=$((selected + 1))
    directory="$output_dir/$name"
    mkdir -p "$directory" || exit 2
    rm -f "$directory/"*.ll "$directory/"*.bc "$directory/"*.log
    stage_errors=()
    "$runner" --case "$name" --output-dir "$directory" > "$directory/run.log" 2> "$directory/assertions.log"
    test_rc=$?
    if [[ $test_rc -eq 2 ]]; then infrastructure_error=1; fi
    if [[ $test_rc -ne 0 ]]; then stage_errors+=("assertions/exit=$test_rc"); fi
    # Preserve independent LLVM evidence even when a project assertion fails.
    llvm_check "$directory" "$directory/module.ll" "$oracle" "$diagnostic"
    if [[ ${#stage_errors[@]} -eq 0 ]]; then record "$name" PASS "assertions, oracle=$oracle"
    else record "$name" FAIL "${stage_errors[*]}"; fi
done < "$output_dir/cases.tsv"
if [[ $selected -eq 0 ]]; then echo "FILTER matched no cases: $filter" >&2; exit 2; fi
coverage_error=0
if [[ "$mode" == coverage ]]; then
    profiles=("$output_dir"/profiles/*.profraw)
    cov_objects=()
    for binary in "${binaries[@]:1}"; do cov_objects+=("-object=$binary"); done
    ir_sources=("$repo_dir"/src/ir/*.cpp "$repo_dir"/src/ir/*.hpp)
    if [[ ${#profiles[@]} -eq 0 ]] || ! "$llvm_profdata" merge -sparse "${profiles[@]}" -o "$output_dir/coverage.profdata" > "$output_dir/coverage.log" 2>&1; then
        coverage_error=1
    else
        "$llvm_cov" report "$runner" "${cov_objects[@]}" -instr-profile="$output_dir/coverage.profdata" "${ir_sources[@]}" > "$output_dir/coverage.txt" 2>> "$output_dir/coverage.log" || coverage_error=1
        "$llvm_cov" export "$runner" "${cov_objects[@]}" -instr-profile="$output_dir/coverage.profdata" "${ir_sources[@]}" > "$output_dir/coverage.json" 2>> "$output_dir/coverage.log" || coverage_error=1
        "$llvm_cov" show "$runner" "${cov_objects[@]}" -instr-profile="$output_dir/coverage.profdata" "${ir_sources[@]}" -format=html -show-branches=count -show-line-counts-or-regions -output-dir="$output_dir/coverage-html" >> "$output_dir/coverage.log" 2>&1 || coverage_error=1
    fi
    if [[ $coverage_error -ne 0 ]]; then echo "Coverage generation failed; see $output_dir/coverage.log" >&2; fi
fi
printf 'Result: %s passed, %s failed, %s selected. Mode: %s. Artifacts: %s\n' "$passed" "$failed" "$selected" "$mode" "$output_dir" | tee -a "$summary"
if [[ $coverage_error -ne 0 || $infrastructure_error -ne 0 ]]; then exit 2; fi
[[ $failed -eq 0 ]]
