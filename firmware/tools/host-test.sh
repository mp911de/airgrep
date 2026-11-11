#!/bin/bash
# Host Unity run, no ESP-IDF activation needed.
#
#   tools/host-test.sh             run every host test
#   tools/host-test.sh charge fusb run the test files whose name contains a word
#
# A test file carries a host main() behind #ifndef ESP_PLATFORM. The source under test is
# the .c or .cpp beside each header the test includes. Files that reach for IDF headers have
# no host build and are skipped. Exit 1 on any failure.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="$(dirname "$HERE")"
# check.sh resolves these before the IDF tools enter PATH, which carries an esp-clang.
HOST_CC="${HOST_CC:-$(command -v cc)}"
HOST_CXX="${HOST_CXX:-$(command -v c++)}"
UNITY="${IDF_PATH:-$HOME/.espressif/v6.1/esp-idf}/components/unity/unity/src"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/airgrep-host.XXXXXX")" || exit 1
trap 'rm -rf "$TMP"' EXIT
rc=0
ok()   { printf 'ok   %-8s %s\n' "$1" "$2"; }
fail() { printf 'FAIL %-8s %s\n' "$1" "$2"; rc=1; }
skip() { printf 'skip %-8s %s\n' "$1" "$2"; }
selected() {
  [ $# -gt 1 ] || return 0
  local name="$1"; shift
  for word in "$@"; do case "$name" in *"$word"*) return 0 ;; esac; done
  return 1
}

idf_only() { grep -lE '#include [<"](esp_|freertos/|driver/|sdkconfig\.h|hal/|soc/)' "$@" > /dev/null 2>&1; }
# Unity itself is C. A C++ driver refuses -std=c++23 for a .c input, so it is compiled once here.
"$HOST_CC" -std=c11 -c -I"$UNITY" "$UNITY/unity.c" -o "$TMP/unity.o" || exit 1
for test in "$ROOT"/components/*/test/test_*.c "$ROOT"/components/*/test/test_*.cpp "$ROOT"/main/test/test_*; do
  [ -f "$test" ] || continue
  name="${test#"$ROOT"/}"
  selected "$(basename "$test")" "$@" || continue
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
exit $rc
