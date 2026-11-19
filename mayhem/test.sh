#!/usr/bin/env bash
#
# mayhem/test.sh — RUN QuickFIX's own Catch2 unit-test suite (`ut`, built by mayhem/build.sh into
# build-test/lib/ut with the project's normal flags). It asserts parsed/serialised message values,
# field conversions, session behaviour, dictionaries, settings, stores, HTTP parsing, ...
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

UT="$SRC/build-test/lib/ut"
if [ ! -x "$UT" ]; then
  echo "test.sh: $UT missing — mayhem/build.sh did not build the unit tests" >&2
  emit_ctrf "catch2" 0 1 0
  exit 1
fi

# Run from a scratch dir: some tests create stores/logs relative to the cwd.
work="$(mktemp -d "${TMPDIR:-/tmp}/qf-ut.XXXXXX")"
log="$work/ut.log"
( cd "$work" && "$UT" --quickfix-config-file "$SRC/test/cfg/ut.cfg" --quickfix-spec-path "$SRC/spec" ) >"$log" 2>&1
rc=$?
tail -n 30 "$log"

# Catch2 summary: "All tests passed (A assertions in N test cases)" or
# "test cases: N | P passed | F failed [| S skipped]".
passed=0; failed=0; skipped=0
if line=$(grep -E '^All tests passed \([0-9]+ assertions? in [0-9]+ test cases?\)' "$log" | tail -n1) && [ -n "$line" ]; then
  passed=$(sed -E 's/.* in ([0-9]+) test case.*/\1/' <<<"$line")
elif line=$(grep -E '^test cases: ' "$log" | tail -n1) && [ -n "$line" ]; then
  total=$(sed -E 's/^test cases: *([0-9]+).*/\1/' <<<"$line")
  passed=$(grep -oE '[0-9]+ passed' <<<"$line" | grep -oE '[0-9]+' || echo 0)
  failed=$(grep -oE '[0-9]+ failed' <<<"$line" | grep -oE '[0-9]+' || echo 0)
  skipped=$(grep -oE '[0-9]+ skipped' <<<"$line" | grep -oE '[0-9]+' || echo 0)
  : "${total:=0}"
else
  echo "test.sh: no Catch2 summary in the ut output (rc=$rc)" >&2
  failed=1
fi
# A non-zero exit with no failed test case counted is still a failure.
if [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi
# No test case ran at all: the runner is broken.
if [ "$passed" -eq 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi
rm -rf "$work"
emit_ctrf "catch2" "$passed" "$failed" "$skipped"
