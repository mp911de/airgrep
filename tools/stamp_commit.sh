#!/usr/bin/env bash
# Writes the current git commit into the KiCad project variable GIT_COMMIT, which the title
# blocks show. Run it right before a fab export, then commit the stamp with the production files.
# A "-dirty" suffix means the export does not match any commit.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
PRO="$ROOT/hardware/kicad/carrier.kicad_pro"
if ls "$ROOT"/hardware/kicad/~*.lck >/dev/null 2>&1; then
  echo "KiCad has the project open. Close it first." >&2
  exit 1
fi
COMMIT="$(git -C "$ROOT" describe --always --dirty --exclude '*' --abbrev=12)"
python3 - "$PRO" "$COMMIT" <<'PY'
import json, re, sys
path, commit = sys.argv[1:]
text = open(path).read()
text, n = re.subn(r'("GIT_COMMIT":\s*)"[^"]*"', lambda m: m.group(1) + json.dumps(commit), text)
if n != 1:
    sys.exit("GIT_COMMIT not found once in %s" % path)
json.loads(text)
open(path, "w").write(text)
PY
echo "GIT_COMMIT = $COMMIT"
