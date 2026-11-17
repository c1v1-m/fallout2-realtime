#!/usr/bin/env bash
# One recorded self-played Fallout 2 Realtime run: takes the shared test lock, runs the scenario (hidden window), then
# MOVES every scr*.bmp the run left in build/Release into <outdir> (in order) and copies the trace.
#   bash rec.sh <scenario path relative to .claude/tools/autotest, without .txt> <outdir>
set -u
REPO=/c/Users/MS/Documents/GameDev/fallout2-ce
NAME="$1"; OUT="$2"
LOCK="$REPO/build/Release/.gamerun.lock"
mkdir -p "$OUT"
until mkdir "$LOCK" 2>/dev/null; do sleep 3; done
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
cd "$REPO"
MARK="$OUT/.marker-$"; touch "$MARK"; sleep 1; before=$(ls build/Release/scr*.bmp 2>/dev/null | wc -l)
node ../fallout1-ce/.claude/tools/autotest/run.js ".claude/tools/autotest/$NAME.txt" 1 --exe "$REPO/build/Release/fallout2-ce.exe" 2>&1 | grep -E 'PASS|FAIL|^run 1'
i=0
for f in $(find build/Release -maxdepth 1 -name "scr*.bmp" -newer "$MARK" | sort); do
    i=$((i+1)); mv "$f" "$OUT/$(printf 'f%04d' $i).bmp"
done
TR=$(ls -t build/Release/autotest_traces/*.log 2>/dev/null | head -1)
[ -n "$TR" ] && cp "$TR" "$OUT/$(basename "$NAME").log"
rm -f "$MARK"
echo "frames: $i (there were $before old scr*.bmp before the run) in $OUT"
