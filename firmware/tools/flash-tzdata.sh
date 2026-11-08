#!/bin/bash
# Flashes tzdb/build/tzdata.bin into the tzdata partition, only when the device holds a
# different table. The 128-byte TZL2 header names release, dataset, cell size and length, so
# an equal header means an equal table and the 1.75 MB write is skipped.
#
#   tools/flash-tzdata.sh -p /dev/cu.usbmodem*          compare and flash if different
#   tools/flash-tzdata.sh -p /dev/cu.usbmodem* --force   flash regardless
#
# Offset and size come from partitions.csv. Needs the IDF environment (esptool).
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="$(dirname "$HERE")"
TABLE="$ROOT/tzdb/build/tzdata.bin"
HEADER_BYTES=128
PORT=""
FORCE=0
while [ $# -gt 0 ]; do
  case "$1" in
    -p) PORT="$2"; shift 2 ;;
    --force) FORCE=1; shift ;;
    *) echo "usage: $0 -p PORT [--force]" >&2; exit 2 ;;
  esac
done
[ -n "$PORT" ] || { echo "usage: $0 -p PORT [--force]" >&2; exit 2; }
[ -f "$TABLE" ] || { echo "FAIL $TABLE missing, run make tzdata in tzdb/" >&2; exit 1; }
read -r OFFSET SIZE < <(awk -F',' '/^tzdata,/ { gsub(/ /, "", $4); gsub(/ /, "", $5); print $4, $5 }' "$ROOT/partitions.csv")
[ -n "$OFFSET" ] || { echo "FAIL no tzdata partition in partitions.csv" >&2; exit 1; }
TABLE_BYTES=$(stat -f %z "$TABLE" 2>/dev/null || stat -c %s "$TABLE")
[ "$TABLE_BYTES" -le "$((SIZE))" ] || { echo "FAIL table $TABLE_BYTES bytes exceeds partition $SIZE" >&2; exit 1; }
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if [ "$FORCE" -eq 0 ]; then
  esptool.py --port "$PORT" read_flash "$OFFSET" "$HEADER_BYTES" "$TMP/device-header.bin" > "$TMP/read.log" 2>&1 || {
    echo "FAIL reading the device header"; cat "$TMP/read.log"; exit 1; }
  head -c "$HEADER_BYTES" "$TABLE" > "$TMP/file-header.bin"
  if cmp -s "$TMP/device-header.bin" "$TMP/file-header.bin"; then
    echo "skip  tzdata on the device matches $TABLE"
    exit 0
  fi
fi
esptool.py --port "$PORT" write_flash "$OFFSET" "$TABLE"
echo "ok    tzdata $TABLE_BYTES bytes written at $OFFSET"
