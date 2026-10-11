#!/bin/bash
# Design check for the carrier board. Read-only: it never saves the board or the schematic.
#
#   tools/check.sh                 run all checks, exit 1 on any finding
#   tools/check.sh --project NAME  check hardware/NAME (default $CARRIER_PROJECT or carrier-ti-charger)
#   tools/check.sh --exports       also check BOM and CPL in the project's jlcpcb/ against the board
#   tools/check.sh --staged        run only for staged design, sim, specs or tools changes (excluding Markdown)
#   tools/check.sh --install-hook  write .git/hooks/pre-commit, a stub that runs this script
#
# Steps: ERC, DRC with zone refill in memory and schematic parity, net names, schematic
# against board per reference, value bindings to specs/power_params.json and the sim
# constants, 3D models (warnings only), sim unit tests. The checks other than the kicad-cli
# runs and the unit tests live in tools/check_design.py.
# Output: one "ok" line per passing step, one "FAIL" line per finding, "warn" lines do not fail.
# Board coordinates are in mil. The check reads the working tree, not the git index.
# CHECK_ROOT points the check at another copy of the repo, for testing the check itself.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT="${CHECK_ROOT:-$(dirname "$HERE")}"
KICAD_APP="${KICAD_APP:-/Applications/KiCad/KiCad.app/Contents}"
CLI="$KICAD_APP/MacOS/kicad-cli"
KPY="$KICAD_APP/Frameworks/Python.framework/Versions/Current/bin/python3"
VPY="$ROOT/hardware/sim/.venv/bin/python"

EXPORTS=""
PROJECT="${CARRIER_PROJECT:-carrier-ti-charger}"
while [ $# -gt 1 ] && [ "$1" = "--project" ]; do PROJECT="$2"; shift 2; done
case "$PROJECT" in /*) ;; *) PROJECT="$ROOT/hardware/$PROJECT" ;; esac
export CARRIER_PROJECT="$PROJECT"
SCH="$PROJECT/carrier.kicad_sch"
PCB="$PROJECT/carrier.kicad_pcb"
[ -f "$PCB" ] || { echo "FAIL project  no carrier.kicad_pcb in $PROJECT"; exit 2; }

case "${1:-}" in
  --exports) EXPORTS="--exports" ;;
  --staged)
    git -C "$ROOT" diff --cached --quiet --no-renames -- \
      hardware/carrier-max-charger/ hardware/carrier-ti-charger/ hardware/sim/ specs/ tools/ ':(glob,exclude)**/*.md'
    status=$?
    case $status in
      0) exit 0 ;;
      1) ;;
      *) echo "FAIL hook     cannot inspect staged changes"; exit "$status" ;;
    esac ;;
  --install-hook)
    hook="$(git -C "$ROOT" rev-parse --git-path hooks/pre-commit)" || exit 1
    case "$hook" in /*) ;; *) hook="$ROOT/$hook" ;; esac
    if [ -e "$hook" ] && ! grep -q 'tools/check.sh' "$hook"; then
      echo "FAIL hook     $hook exists and is not this hook"; exit 1
    fi
    printf '#!/bin/sh\nexec "$(git rev-parse --show-toplevel)/tools/check.sh" --staged\n' > "$hook" && chmod +x "$hook"
    echo "ok   hook     $hook"; exit 0 ;;
  "") ;;
  *) sed -n '2,8p' "$0"; exit 2 ;;
esac

TMP="$(mktemp -d "${TMPDIR:-/tmp}/flarm-check.XXXXXX")" || exit 1
trap 'rm -rf "$TMP"' EXIT
rc=0

# The unit tests take the longest. They run in the background while KiCad works.
if [ -x "$VPY" ]; then
  PYTHONDONTWRITEBYTECODE=1 "$VPY" -m unittest discover -s "$ROOT/hardware/sim" -p 'test_*.py' > "$TMP/tests.log" 2>&1 &
  tests_pid=$!
fi

# kicad-cli prints Fontconfig and wx noise. Its output shows only when a run fails.
cli() {
  local name="$1"; shift
  if ! "$CLI" "$@" > "$TMP/$name.log" 2>&1; then
    printf 'FAIL %-8s kicad-cli failed: %s\n' "$name" "$(grep -viE 'fontconfig|debug|^$' "$TMP/$name.log" | tail -1)"
    return 1
  fi
}
cli erc sch erc --format json --units mils --severity-all -o "$TMP/erc.json" "$SCH" || rc=1
# No --save-board: the zones refill in memory only.
cli drc pcb drc --format json --units mils --severity-all --schematic-parity --refill-zones \
  -o "$TMP/drc.json" "$PCB" || rc=1
cli netlist sch export netlist --format kicadsexpr -o "$TMP/sch.net" "$SCH" || rc=1

if [ $rc -eq 0 ]; then
  PYTHONDONTWRITEBYTECODE=1 "$KPY" "$HERE/check_design.py" "$ROOT" "$TMP" $EXPORTS 2> "$TMP/design.log" || rc=1
  if grep -q '^Traceback' "$TMP/design.log"; then
    echo "FAIL design   check_design.py crashed: $(tail -1 "$TMP/design.log")"
  fi
fi

if [ -x "$VPY" ]; then
  if wait "$tests_pid"; then
    echo "ok   tests    $(grep -E '^Ran [0-9]+ tests?' "$TMP/tests.log")"
  else
    rc=1
    grep -E '^(FAIL|ERROR): ' "$TMP/tests.log" | sed 's/^/FAIL tests    /'
    grep -qE '^(FAIL|ERROR): ' "$TMP/tests.log" || echo "FAIL tests    $(tail -1 "$TMP/tests.log")"
  fi
else
  rc=1
  echo "FAIL tests    no venv at hardware/sim/.venv, see hardware/sim/README.md"
fi

[ $rc -eq 0 ] && echo "check passed" || echo "check FAILED"
exit $rc
