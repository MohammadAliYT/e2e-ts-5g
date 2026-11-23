#!/usr/bin/env bash
# Handover table for oai-ue-ho-ssb-cancel.patch: tools/hub-ho-test.sh at several gaps (target weaker than the
# source at the handover), with and without DL load. Every case restarts the lab. About 4 minutes per case.
# Usage: sudo -v && (sudo setsid nohup tools/ho-gap-table.sh > /tmp/ho-table.log 2>&1 < /dev/null &)
#   CASES='<gap>:<load> ...'   default '0:- 3:- 6:- 3:10M 6:10M' (load - = no load; 0:- = regression)
# Result: the hub-ho-test lines, one line per UE with the cancel and T304 counts, UE logs in results/hub-ho/table-<time>/
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out="$root/results/hub-ho/table-$(date +%Y%m%d-%H%M%S)"; mkdir -p "$out"
for c in ${CASES:-0:- 3:- 6:- 3:10M 6:10M}; do
  g=${c%%:*} l=${c#*:}
  echo "=== HO_GAP=$g dB, DL load ${l/-/none}"
  if [[ $l == - ]]; then HO_GAP=$g tools/hub-ho-test.sh; else HO_GAP=$g HO_LOAD=$l tools/hub-ho-test.sh; fi
  for u in 1 2; do
    f=results/f1-smoke/ue$u/ue$u.log
    cp "$f" "$out/gap$g-load$l-ue$u.log" 2>/dev/null
    echo "  ue$u: cancel used $(grep -c 'ssb-cancel.*used 1' "$f"), PBCH decoded $(grep -c 'ssb-cancel.*PBCH decoded' "$f"), not decoded $(grep -c 'ssb-cancel.*not decoded' "$f"), T304 $(grep -c 'T304' "$f")"
  done
done
echo "logs: $out"