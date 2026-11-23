#!/bin/bash
# The gate for a change to the firmware or the utility. Writes only to the build directories.
#
#   tools/check.sh                 run all steps, exit 1 on any finding
#   tools/check.sh --preset NAME   build only that preset, the other steps still run
#
# Steps: every preset in CMakePresets.json builds with idf.py, which checks the formatting, runs
# clang-tidy with .clang-tidy and fails when the linker left a test out, then ctest runs every test
# of the host build: utility/build on macOS, build/host elsewhere. ctest -L display, firmware, device, lint or docs runs one group of them.
# Output: one "ok" line per passing step of this script, one "FAIL" line per finding, and what ctest
# prints.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="$(dirname "$HERE")"
IDF_ACTIVATE="${IDF_ACTIVATE:-$HOME/.espressif/tools/activate_idf_v6.1.sh}"
# The host build resolves its tools before the IDF tools enter PATH, which carries an esp-clang and a
# Python of its own.
HOST_CMAKE="$(command -v cmake)"
HOST_PATH="$PATH"

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

presets="$(python3 -c 'import json, sys
print(*(preset["name"] for preset in json.load(open(sys.argv[1]))["buildPresets"]))' "$ROOT/CMakePresets.json")"
[ -n "$PRESET" ] && presets="$PRESET"
[ -n "$presets" ] || fail build "no build presets found"
if ! command -v idf.py > /dev/null 2>&1; then
  fail build "idf.py not found, activate ESP-IDF or set IDF_ACTIVATE"
else
  for preset in $presets; do
    if (cd "$ROOT" && idf.py --preset "$preset" build > "$TMP/build-$preset.log" 2>&1); then
      ok build "$preset"
    else
      fail build "$preset"
      grep -m5 -E 'error:|CMake Error|FAILED:|Dropped by the linker' "$TMP/build-$preset.log" | sed "s|$ROOT/||"
    fi
  done
fi

# The host build and its tests. The utility is a macOS application and includes tests/host/.
case "$(uname)" in
  Darwin) host="$ROOT/utility/build"; configure="cmake -S utility -B utility/build -G Ninja" ;;
  *) host="$ROOT/build/host"; configure="cmake -S tests/host -B build/host -G Ninja" ;;
esac
if [ ! -f "$host/build.ninja" ] && ! (cd "$ROOT" && PATH="$HOST_PATH" $configure > "$TMP/configure.log" 2>&1); then
  fail host "configure  $(grep -m1 -i 'error' "$TMP/configure.log" | cut -c1-160)"
elif ! PATH="$HOST_PATH" "$HOST_CMAKE" --build "$host" > "$TMP/host.log" 2>&1; then
  fail host "build  $(grep -m1 'error' "$TMP/host.log" | cut -c1-160)"
elif PATH="$HOST_PATH" ctest --test-dir "$host" --no-tests=error --output-on-failure; then
  ok host "ctest"
else
  fail host "ctest"
fi

[ $rc -eq 0 ] && echo "check passed" || echo "check FAILED"
exit $rc
