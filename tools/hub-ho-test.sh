#!/usr/bin/env bash
# Hub handover test (H3): 2 UEs, 2 DUs, handover after realistic movement.
# Starts the lab in hub mode (UE1 close to DU1, UE2 close to DU2), then for each UE and each direction:
#   1. move toward the target DU in steps (path loss via the hub control port):
#      target 6 dB weaker -> 3 dB weaker -> equal (crossing point)
#   2. guard: still on the source DU? (if the link already failed, ho.sh would send the UE to the
#      wrong DU, because it always picks "the other DU") -> otherwise skip the handover
#   3. E2 handover at the crossing point (tools/ho.sh <CU UE ID>), then the UE keeps moving:
#      source 10 dB weaker; check serving DU and ping
# PASS = UE in-sync on the target DU only, ping loss <= 20 %, no re-establishment (no bounce).
# Usage: tools/hub-ho-test.sh [--no-start]      (--no-start: lab already up in hub mode, 2 UEs)
# HO_TOOL=<script>: handover tool with the interface of tools/ho.sh (default tools/ho.sh);
#   HO_TOOL=tools/ho-ctrl.sh sends the handover through the running controller (phase 3).
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
out="$root/results/hub-ho"; mkdir -p "$out"
log="$out/run-$(date +%Y%m%d-%H%M%S).txt"

ctrl()    { echo "$*" | sudo ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 4099; }
serving() { # DUs on which CU UE ID $1 is in-sync
  local s=""
  for d in du1 du2; do
    grep -q "CU-UE-ID $1 in-sync" "results/f1-smoke/$d/nrMAC_stats.log" 2>/dev/null && s+="$d,"
  done
  s=${s%,}; echo "${s:-none}"; }
loss_pct() { # ping loss in % for UE $1 (100 if no answer at all)
  local ns=UE${1}_NS p
  p=$(sudo ip netns exec "${!ns}" ping -c 10 -i 0.2 -W 1 -I "ue$1" "$DN_IP" 2>/dev/null \
      | grep -o '[0-9.]*% packet loss' | cut -d% -f1)
  echo "${p:-100}"; }
reest() { # number of re-establishments in UE $1's log (a bounce shows up here)
  local n; n=$(grep -c 'Received RRCReestablishment' "results/f1-smoke/ue$1/ue$1.log" 2>/dev/null); echo "${n:-0}"; }

ho_test() { # <ue number> <CU UE ID> <from DU> <to DU>
  local u=$1 id=$2 a=$3 b=$4 r0 r1 s0 s1 s2 p2 t0 t1 ho_s="-" verdict=FAIL
  r0=$(reest "$u")
  ctrl loss "ue$u" "du$b" 6 >/dev/null; sleep 2     # target 6 dB weaker
  ctrl loss "ue$u" "du$b" 3 >/dev/null; sleep 2     # target 3 dB weaker
  ctrl loss "ue$u" "du$b" 0 >/dev/null              # crossing point: both cells equally strong
  s0=$(serving "$id")
  if [[ $s0 == "du$a" ]]; then
    t0=$(date +%s.%N)
    "${HO_TOOL:-$root/tools/ho.sh}" "$id" >>"$log" 2>&1
    t1=$(date +%s.%N)
    ho_s=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.1f", b - a }')
  else
    echo "  ue$u not on du$a before the handover (serving: $s0) - handover skipped" | tee -a "$log"
  fi
  s1=$(serving "$id")
  ctrl loss "ue$u" "du$a" 10 >/dev/null; sleep 3    # UE moves on, close to the target
  s2=$(serving "$id"); p2=$(loss_pct "$u")
  r1=$(reest "$u")
  if [[ $s2 == "du$b" && ${p2%.*} -le 20 && $r1 -eq $r0 ]]; then verdict=PASS; fi
  printf '%-4s %-10s %-6s %-9s %-9s %-6s %-6s %s\n' \
    "ue$u" "du$a->du$b" "${ho_s}s" "$s1" "$s2" "$p2%" "$((r1 - r0))" "$verdict" | tee -a "$log"
}

if [[ ${1:-} != --no-start ]]; then
  tools/lab-down.sh
  HUB_DUS=2 HUB_UES=2 HUB_INIT='loss ue1 du2 10;loss ue2 du1 10' LAB_MODE=hub tools/lab-up.sh || exit 1
  sleep 5
fi

echo "# $(date -Is) hub handover test" | tee -a "$log"
echo "start: ue1 on $(serving 1), ue2 on $(serving 2)" | tee -a "$log"
printf '%-4s %-10s %-6s %-9s %-9s %-6s %-6s %s\n' \
  ue move "HO" "after HO" "close" loss reest result | tee -a "$log"
ho_test 1 1 1 2
ho_test 1 1 2 1
ho_test 2 2 2 1
ho_test 2 2 1 2
ctrl show | tee -a "$log"
echo "log: $log"