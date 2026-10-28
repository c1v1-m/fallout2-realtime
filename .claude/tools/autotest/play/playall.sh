#!/bin/sh
# Plays every scenario in this folder (or the ones named) one after another and writes, per scenario,
# the run.js report and the trace into $OUT, plus one summary line each into $OUT/summary.txt.
#   sh .claude/tools/autotest/play/playall.sh [names...]     (from fallout2-ce)
cd "$(dirname "$0")/../../../.." || exit 1
PLAY=.claude/tools/autotest/play
OUT=${OUT:-build/Release/play_results}
EXE=${EXE:-build/Release/fallout2-ce.exe}
mkdir -p "$OUT"
if [ $# -gt 0 ]; then list="$*"; else list=$(ls $PLAY/*.txt | xargs -n1 basename | sed 's/\.txt$//'); fi
for s in $list; do
  timeout 300 node ../fallout1-ce/.claude/tools/autotest/run.js $PLAY/$s.txt 1 --exe $EXE > "$OUT/$s.txt" 2>&1
  code=$?
  cp build/Release/autotest_traces/${s}_1.log "$OUT/$s.log" 2>/dev/null
  run=$(grep -a -m1 '######## run' "$OUT/$s.txt" | sed 's/######## run 1\/1  //')
  dude=$(grep -a -m1 '   dude:' "$OUT/$s.txt" | sed 's/^ *//')
  bad=$(grep -a -c '\[BAD' "$OUT/$s.txt")
  err=$(grep -a -c '\[ERR/' "$OUT/$s.log" 2>/dev/null)
  warn=$(grep -a '\[WARN/' "$OUT/$s.log" 2>/dev/null | grep -a -v 'wield-failed' | wc -l)
  hang=$(grep -a -c '=== HANG\|=== CRASH' "$OUT/$s.log" 2>/dev/null)
  echo "$s | node=$code | $run | $dude | BAD=$bad ERR=$err WARN=$warn HANG/CRASH=$hang" | tee -a "$OUT/summary.txt"
done
echo done
