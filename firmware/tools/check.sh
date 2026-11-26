#!/bin/bash
# The gate for a change to the firmware or the utility. Writes only to the build directories.
#
#   tools/check.sh                 run all steps, exit 1 on any finding
#   tools/check.sh --preset NAME   build only that preset, the other steps still run
#
# Steps: the firmware workflows in PRESETS, whose builds check the formatting, run clang-tidy and fail
# when the linker left a test out, then the host workflow, which builds and runs ctest: utility-macos
# on macOS, tests/host elsewhere.
# Output: one "ok" line per passing step, one "FAIL" line per finding, and what ctest prints.
set -u
PRESETS="s3 s3+service s3+800x480-touch s3+800x480-touch+debug s3+test"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="$(dirname "$HERE")"
IDF_ACTIVATE="${IDF_ACTIVATE:-$HOME/.espressif/tools/activate_idf_v6.1.sh}"
# The host build resolves its tools before the IDF tools enter PATH, which carries an esp-clang and a
# Python of its own.
HOST_PATH="$PATH"

case "${1:-}" in
  --preset) PRESETS="${2:-}"; [ -n "$PRESETS" ] || { sed -n '2,5p' "$0"; exit 2; } ;;
  "") ;;
  *) sed -n '2,5p' "$0"; exit 2 ;;
esac

TMP="$(mktemp -d "${TMPDIR:-/tmp}/airgrep-check.XXXXXX")" || exit 1
trap 'rm -rf "$TMP"' EXIT
rc=0
ok()   { printf 'ok   %-8s %s\n' "$1" "$2"; }
fail() { printf 'FAIL %-8s %s\n' "$1" "$2"; rc=1; }
# Without an activated shell the IDF activation script's -e mode supplies the environment. It cannot
# be sourced from a script, it exits when $0 is not a shell name.
if [ -z "${IDF_PATH:-}" ] && [ -f "$IDF_ACTIVATE" ]; then
  while IFS='=' read -r key value; do
    case "$key" in
      PATH) export PATH="$value:$PATH" ;;
      SYSTEM_PATH) ;;
      *) export "$key=$value" ;;
    esac
  done < <(sh "$IDF_ACTIVATE" -e)
fi

for preset in $PRESETS; do
  if (cd "$ROOT" && cmake --workflow --preset "$preset" > "$TMP/build-$preset.log" 2>&1); then
    ok build "$preset"
  else
    fail build "$preset"
    grep -m5 -E 'error:|CMake Error|FAILED:|Dropped by the linker' "$TMP/build-$preset.log" | sed "s|$ROOT/||"
  fi
done

case "$(uname)" in
  Darwin) cd "$ROOT/utility" && configure=(cmake --preset utility-macos) build=(cmake --build --preset utility-macos) test=(ctest --preset utility-macos) ;;
  *) cd "$ROOT" && configure=(cmake -S tests/host -B build/host -G Ninja) build=(cmake --build build/host) test=(ctest --test-dir build/host --no-tests=error --output-on-failure) ;;
esac
if ! PATH="$HOST_PATH" "${configure[@]}" > "$TMP/configure.log" 2>&1; then
  fail host "configure  $(grep -m1 -i 'error' "$TMP/configure.log" | cut -c1-160)"
elif ! PATH="$HOST_PATH" "${build[@]}" > "$TMP/host.log" 2>&1; then
  fail host "build  $(grep -m1 'error' "$TMP/host.log" | cut -c1-160)"
elif PATH="$HOST_PATH" "${test[@]}"; then
  ok host "ctest"
else
  fail host "ctest"
fi

[ $rc -eq 0 ] && echo "check passed" || echo "check FAILED"
exit $rc
