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

# Host Unity run.
HOST_CC="$HOST_CC" HOST_CXX="$HOST_CXX" "$HERE/host-test.sh" || rc=1

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
