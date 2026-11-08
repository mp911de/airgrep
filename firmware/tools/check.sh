#!/bin/bash
# Software floor for firmware changes. Read-only apart from the preset build directories.
#
#   tools/check.sh                 run all checks, exit 1 on any finding
#   tools/check.sh --preset NAME   build only that preset, the other checks still run
#
# Steps: every preset in CMakePresets.json builds with idf.py, each test file with a host
# main() block compiles against the IDF Unity sources on the host and runs, every tag in
# CONFIG_AIRGREP_TEST_TAGS of sdkconfig.test has a TEST_CASE, every test object compiled
# for a test preset survives the link into airgrep.map, no source includes "../".
# Output: one "ok" line per passing step, one "FAIL" line per finding, "skip" lines name
# test files that need IDF headers and have no host run. The check reads the working tree.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="$(dirname "$HERE")"
IDF_ACTIVATE="${IDF_ACTIVATE:-$HOME/.espressif/tools/activate_idf_v6.1.sh}"
# Host compilers resolve before the IDF tools enter PATH, which carries an esp-clang.
HOST_CC="$(command -v cc)"
HOST_CXX="$(command -v c++)"

PRESET=""
case "${1:-}" in
  --preset) PRESET="${2:-}"; [ -n "$PRESET" ] || { sed -n '2,5p' "$0"; exit 2; } ;;
  "") ;;
  *) sed -n '2,5p' "$0"; exit 2 ;;
esac

TMP="$(mktemp -d "${TMPDIR:-/tmp}/airgrep-check.XXXXXX")" || exit 1
trap 'rm -rf "$TMP"' EXIT
rc=0
ok()   { printf 'ok   %-8s %s\n' "$1" "$2"; }
fail() { printf 'FAIL %-8s %s\n' "$1" "$2"; rc=1; }
skip() { printf 'skip %-8s %s\n' "$1" "$2"; }

# Build presets. Without an activated shell the IDF activation script's -e mode supplies the
# environment. It cannot be sourced from a script, it exits when $0 is not a shell name.
if ! command -v idf.py > /dev/null 2>&1 && [ -f "$IDF_ACTIVATE" ]; then
  while IFS='=' read -r key value; do
    case "$key" in
      PATH) export PATH="$value:$PATH" ;;
      SYSTEM_PATH) ;;
      *) export "$key=$value" ;;
    esac
  done < <(sh "$IDF_ACTIVATE" -e)
fi
presets="$(sed -n '/"buildPresets"/,$p' "$ROOT/CMakePresets.json" | grep -o '"name": *"[^"]*"' | sed 's/.*"\([^"]*\)"$/\1/')"
[ -n "$PRESET" ] && presets="$PRESET"
if ! command -v idf.py > /dev/null 2>&1; then
  fail build "idf.py not found, activate ESP-IDF or set IDF_ACTIVATE"
else
  for preset in $presets; do
    if (cd "$ROOT" && idf.py --preset "$preset" build > "$TMP/build-$preset.log" 2>&1); then
      ok build "$preset"
    else
      # A compiler error is one line, a CMake error is a paragraph.
      reason="$(grep -m1 -E '^[^ ]+:[0-9]+:[0-9]+: (fatal )?error:' "$TMP/build-$preset.log" | sed "s|$ROOT/||")"
      [ -n "$reason" ] || reason="$(awk '/CMake Error|FAILED:/{p=1} p&&/Call Stack|^ninja/{exit} p&&NF{print}' "$TMP/build-$preset.log" | tr -s ' \n' ' ')"
      fail build "$preset  $(echo "$reason" | cut -c1-200)"
    fi
  done
fi

# Host Unity run. A test file carries a host main() behind #ifndef ESP_PLATFORM. The source
# under test is the .c or .cpp beside each header the test includes. Files that reach for IDF
# headers have no host build and are skipped.
UNITY="${IDF_PATH:-$HOME/.espressif/v6.1/esp-idf}/components/unity/unity/src"
idf_only() { grep -lE '#include [<"](esp_|freertos/|driver/|sdkconfig\.h|hal/|soc/)' "$@" > /dev/null 2>&1; }
# Unity itself is C. A C++ driver refuses -std=c++23 for a .c input, so it is compiled once here.
"$HOST_CC" -std=c11 -c -I"$UNITY" "$UNITY/unity.c" -o "$TMP/unity.o"
for test in "$ROOT"/components/*/test/test_*.c "$ROOT"/components/*/test/test_*.cpp "$ROOT"/main/test/test_*; do
  [ -f "$test" ] || continue
  name="${test#"$ROOT"/}"
  grep -q '#ifndef ESP_PLATFORM' "$test" || { skip host "$name  no host main()"; continue; }
  testdir="$(dirname "$test")"
  comp="$(dirname "$testdir")"
  sources=""; headers=""; incs="-I$comp -I$testdir"
  for header in $(grep -o '#include "[^"]*\.h\(pp\)\?"' "$test" | sed 's/#include "\(.*\)"/\1/'); do
    for dir in "$testdir" "$comp" "$comp"/*/; do
      [ -f "$dir/$header" ] || continue
      path="$(cd "$(dirname "$dir/$header")" && pwd -P)/$(basename "$header")"
      headers="$headers $path"; incs="$incs -I$(dirname "$path")"
      for src in "${path%.*}.c" "${path%.*}.cpp"; do [ -f "$src" ] && sources="$sources $src"; done
      break
    done
  done
  # shellcheck disable=SC2086
  if idf_only "$test" $headers $sources; then skip host "$name  needs IDF headers"; continue; fi
  bin="$TMP/$(basename "${test%.*}")"
  case "$test" in
    *.cpp) compile=("$HOST_CXX" -std=c++23 -Wall -Wshadow $incs -I"$UNITY" "$test" $sources "$TMP/unity.o") ;;
    *)     compile=("$HOST_CC" -std=c11 -Wall -Wshadow $incs -I"$UNITY" "$test" $sources "$UNITY/unity.c") ;;
  esac
  if ! "${compile[@]}" -o "$bin" > "$bin.log" 2>&1; then
    fail host "$name  $(grep -m1 'error:' "$bin.log" | sed "s|$ROOT/||" | cut -c1-160)"
  elif "$bin" > "$bin.out" 2>&1; then
    ok host "$name  $(grep -E '^[0-9]+ Tests' "$bin.out")"
  else
    fail host "$name  $(grep -m1 ':FAIL' "$bin.out" | sed "s|$ROOT/||")"
  fi
done

# Tag coverage: every tag the test build runs at boot must name at least one TEST_CASE.
tags="$(sed -n 's/^CONFIG_AIRGREP_TEST_TAGS="\(.*\)"$/\1/p' "$ROOT/sdkconfig.test")"
for tag in $tags; do
  if grep -rqF "\"$tag\"" "$ROOT"/components/*/test "$ROOT"/main/test; then
    ok tags "$tag"
  else
    fail tags "$tag  no TEST_CASE carries it"
  fi
done

# Linker retention: each test object a test preset compiles must appear in its airgrep.map.
# The linker drops TEST_CASE registrations from an archive nobody references.
for preset in $(sed -n '/"configurePresets"/,/"buildPresets"/p' "$ROOT/CMakePresets.json" | grep -B12 '"AIRGREP_TEST": *"ON"' | grep -o '"name": *"[^"]*"' | sed 's/.*"\([^"]*\)"$/\1/'); do
  [ -z "$PRESET" ] || [ "$preset" = "$PRESET" ] || continue
  ninja="$ROOT/build/$preset/build.ninja"; map="$ROOT/build/$preset/airgrep.map"
  [ -f "$ninja" ] || { skip link "$preset  not configured"; continue; }
  [ -f "$map" ] || { fail link "$preset  no airgrep.map, the build did not link"; continue; }
  missing=""
  for obj in $(grep -o '^build [^:]*/test/\?[^/:]*test_[^/:]*\.obj' "$ninja" | sed 's|.*/||' | sort -u); do
    grep -qF "$obj" "$map" || missing="$missing $obj"
  done
  if [ -z "$missing" ]; then
    ok link "$preset"
  else
    hint="a main component test needs WHOLE_ARCHIVE or a test component"
    [ "$map" -ot "$ninja" ] && hint="map is older than build.ninja, the preset did not build"
    fail link "$preset  dropped by the linker:$missing  ($hint)"
  fi
done

# Include hygiene: headers resolve through component include dirs, never through "../".
if grep -rn '#include "\.\./' "$ROOT/main" "$ROOT/components" > "$TMP/includes" 2>/dev/null; then
  sed "s|^$ROOT/||; s/:\(.*\)$/  \1/" "$TMP/includes" | while IFS= read -r line; do fail include "$line"; done
  rc=1
else
  ok include "no \"../\" includes"
fi

[ $rc -eq 0 ] && echo "check passed" || echo "check FAILED"
exit $rc
