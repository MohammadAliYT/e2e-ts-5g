#!/usr/bin/env bash
# Handover of one UE to the other DU, sent by the running controller (e2e_ctrl, phase 3).
# Same interface as tools/ho.sh, so tools/hub-ho-test.sh can use it:
#   HO_TOOL=tools/ho-ctrl.sh tools/hub-ho-test.sh --no-start
# Usage: tools/ho-ctrl.sh <CU UE ID>
# 1. find the serving DU (nrMAC_stats.log, "CU-UE-ID <id> in-sync")
# 2. write "ho <id> <other DU>" to the controller's command FIFO (explicit target cell)
# 3. wait until the UE is in-sync on the target DU only (max HO_WAIT_S, default 8 s)
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
fifo=${E2E_CMD:-/tmp/e2e_ctrl.cmd}
S=$root/results/f1-smoke
[[ $# -eq 1 ]] || { echo "usage: $0 <CU-UE-ID>"; exit 1; }
id=$1
[[ -p $fifo ]] || { echo "no command FIFO $fifo - is e2e_ctrl running?"; exit 1; }

serving() {
  local s=""
  for d in du1 du2; do grep -q "CU-UE-ID $id in-sync" "$S/$d/nrMAC_stats.log" 2>/dev/null && s+="$d,"; done
  s=${s%,}; echo "${s:-none}"; }

src=$(serving)
case $src in
  du1) to=2 ;;
  du2) to=1 ;;
  *) echo "CU UE ID $id: serving DU unclear ($src) - no handover"; exit 1 ;;
esac
echo "CU UE ID $id: $src -> du$to (command to the controller)"
t0=$(date +%s.%N)
echo "ho $id $to" > "$fifo"
for _ in $(seq 1 $(( ${HO_WAIT_S:-8} * 5 ))); do
  sleep 0.2
  if [[ $(serving) == "du$to" ]]; then
    awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN { printf "on du'"$to"' after %.1f s (MAC stats)\n", b - a }'
    exit 0
  fi
done
echo "CU UE ID $id: not on du$to after ${HO_WAIT_S:-8} s (serving: $(serving))"
exit 1
