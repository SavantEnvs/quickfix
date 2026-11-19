#!/usr/bin/env bash
#
# mayhem/build.sh — build QuickFIX's fuzz harnesses and its unit-test suite.
#
#   build-fuzz/  libquickfix.a with $SANITIZER_FLAGS + SanCov, the six harnesses (fuzzer +
#                -standalone) linked against it -> /mayhem/<fuzzer>, /mayhem/<fuzzer>-standalone
#   build-test/  upstream's Catch2 unit-test runner `ut` with the project's normal flags (run by test.sh)
# Both build dirs match upstream's `build*` .gitignore entry, so `git clean -ffdX` removes them.
# Upstream's CMake also (re)writes src/C++/config.h, src/C++/Allocator.h and include/quickfix/ in
# the source tree on every configure; all of that is upstream behaviour and regenerated here.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

: "${SRC:=/mayhem}"
cd "$SRC"
OUT="${OUT:-/mayhem}"

# UBSan's vptr check is relaxed (only it): QuickFIX static_casts plain FieldBase objects to the
# typed field classes throughout (e.g. Message.cpp `static_cast<const BodyLength &>(field)`), so
# -fsanitize=vptr reports a "downcast of address ... which does not point to an object of type" on
# every input, including loading the bundled dictionaries. ASan and the rest of UBSan stay halting.
FUZZ_SAN="$SANITIZER_FLAGS -fno-sanitize=vptr"

HARNESSES="fuzz_message fuzz_session fuzz_datadictionary fuzz_http fuzz_settings fuzz_message_nodict"
SPECS="FIX42 FIX44 FIXT11 FIX50SP2"

# 1) The library, instrumented (sanitizers + SanCov edges) with DWARF 3.
cmake -S . -B build-fuzz -G Ninja \
  -DCMAKE_BUILD_TYPE=None \
  -DQUICKFIX_SHARED_LIBS=OFF -DQUICKFIX_EXAMPLES=OFF -DQUICKFIX_TESTS=OFF \
  -DQUICKFIX_LIB_OUTPUT_DIR="$SRC/build-fuzz/lib" \
  -DCMAKE_C_FLAGS="$FUZZ_SAN -fsanitize=fuzzer-no-link -O1 $DEBUG_FLAGS" \
  -DCMAKE_CXX_FLAGS="$FUZZ_SAN -fsanitize=fuzzer-no-link -O1 $DEBUG_FLAGS"
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target quickfix
LIB="$SRC/build-fuzz/lib/libquickfix.a"

# 2) Embedded dictionaries: spec/<NAME>.xml -> build-fuzz/specinc/spec_<NAME>.inc (C byte list).
mkdir -p build-fuzz/specinc
for s in $SPECS; do
  od -An -v -tu1 "spec/$s.xml" | tr -s ' \n' ',' | sed 's/^,//; s/,$//' > "build-fuzz/specinc/spec_$s.inc.tmp"
  mv "build-fuzz/specinc/spec_$s.inc.tmp" "build-fuzz/specinc/spec_$s.inc"
done

INC="-I$SRC/src/C++ -I$SRC/mayhem/harnesses -I$SRC/build-fuzz/specinc"
CXXF="-std=c++17 -O1 $FUZZ_SAN $DEBUG_FLAGS"
mkdir -p build-fuzz/obj
$CXX $FUZZ_SAN $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o build-fuzz/obj/lsan_off.o
$CC $FUZZ_SAN $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/obj/standalone_main.o
for h in $HARNESSES; do
  $CXX $CXXF -fsanitize=fuzzer-no-link $INC -c "$SRC/mayhem/harnesses/$h.cpp" -o "build-fuzz/obj/$h.o"
  $CXX $CXXF $LIB_FUZZING_ENGINE "build-fuzz/obj/$h.o" build-fuzz/obj/lsan_off.o "$LIB" -lpthread -o "$OUT/$h"
  $CXX $CXXF build-fuzz/obj/standalone_main.o "build-fuzz/obj/$h.o" build-fuzz/obj/lsan_off.o "$LIB" -lpthread \
    -o "$OUT/$h-standalone"
done

# 3) Upstream's unit tests (Catch2 `ut`), normal flags, for test.sh.
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -S . -B build-test -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQUICKFIX_SHARED_LIBS=OFF -DQUICKFIX_EXAMPLES=OFF -DQUICKFIX_TESTS=ON \
  -DQUICKFIX_LIB_OUTPUT_DIR="$SRC/build-test/lib" \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build build-test -j"$MAYHEM_JOBS" --target ut
test -x build-test/lib/ut
echo "build.sh: done"
