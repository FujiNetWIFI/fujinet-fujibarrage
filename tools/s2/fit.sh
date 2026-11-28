#!/usr/bin/env bash
# fit.sh -- every RFIT in src/studio2 set to its routine's size: all of them
# first at 256 (each routine starts a page), a TEST build measured, the
# sizes written by fitfix.py, then the build again. A developer's tool.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
sed -i -E 's/^(\s*RFIT\s+)[0-9]+/\1256/' "$ROOT"/src/studio2/src/*.inc "$ROOT"/src/studio2/barrage.asm
OUT="$ROOT/build/studio2-fit"
rm -f "$OUT"/barrage.b*.lst
FITALL=1 TEST=1 OUT="$OUT" R2R="$OUT" "$ROOT/src/studio2/build.sh" >/dev/null 2>&1 || true
ls "$OUT"/barrage.b*.lst >/dev/null 2>&1 || { echo "fit.sh: no listing" >&2; exit 1; }
python3 "$ROOT/tools/s2/fitfix.py" "$OUT"/barrage.b*.lst
TEST=1 OUT="$OUT" R2R="$OUT" "$ROOT/src/studio2/build.sh"
