#!/usr/bin/env bash
# Neighbour measurement test (hub mode). Needs the UE diag lines of oai-ue-neigh-known-pci.patch:
# start the lab with E2E_NEIGH_DIAG=1 tools/lab-up.sh (the switch is read when the UE starts; the script
# cannot switch it on in a running UE, so it stops if a UE log does not show "neighbour diag lines on").
# Start positions: UE1 next to DU1, UE2 next to DU2 (other DU 10 dB weaker for each UE).
# The script changes the hub loss of ONE UE to its neighbour DU, step by step:
#   MOVE=ue1 (default): loss ue1 du2 (UE1 watches DU2 = PCI 1); UE2 is the control
#   MOVE=ue2:           loss ue2 du1 (UE2 watches DU1 = PCI 0); UE1 is the control
# Per step and per UE it prints (counted after SETTLE seconds; LOST and ADD over the whole step):
#   reports  MeasurementReport sizes: 53 bits = serving cell only, 82 bits = serving + 1 neighbour
#   LOST     neighbour dropped after 10 failed checks (UE sends INT_MAX, RRC removes the neighbour)
#   ADD      neighbour added (ratio > 3 on 2 checks in a row)
#   conf     "confirmed" diag lines (1 line = 5 successful checks, about 1 per second)
#   nodet    "not detected" diag lines (1 line = 5 failed checks)
#   timing   chosen PSS timing in the slot with counts (S = found by the PSS/SSS search fallback); expected 4560
# For the moving UE: CFAR ratio (median / max / min) of the checks that tested a timing, the threshold in use
# (add 3 / keep 1.5), and how many diag lines tested a timing (a cell that is not valid is checked every
# 21 slots in any slot, so most of these checks find no SSB and test no timing),
# and its last diag line (rho, RSRP per cell vs all SSS power).
# Per step, reported values for the moving UE: UE PHY (serving: MAC stats "RSRP", neighbour: "sent") and the
# CU's last periodic MeasurementReport (nrRRC_stats.log, printed as index - 156 = upper edge of the 1 dB bin;
# only periodic reports update it). Gap = serving - neighbour; target: gap = loss +-1 dB at 0-10 dB.
# Pass: 0-10 dB: 82 b only, LOST 0, timing 4560, RSRP gap cell vs all SSS follows the loss
#       (10 -> ~10, 6 -> ~6, 3 -> ~4.8, 0 -> ~3 dB); 25 dB: neighbour detected (kept, and added again after 60 dB);
#       60 dB: LOST 1, then 53 b only and conf 0.
# Only this script may change the hub while it runs. The moved loss is set back to 10 dB at the end.
# With CTRL_CSV=<controller CSV> (controller started with E2E_CSV=<same path>) it also prints the controller's
# last values for the moving UE (phase 1b; target: controller = CU - 1 dB, same gap).
# Usage: [MOVE=ue1|ue2] [SETTLE=5] [CTRL_CSV=...] tools/neigh-meas-test.sh [default seconds per step] ["loss[:seconds] ..."]
#   default steps: "10:40 6:40 3:40 0:20 10:40 25:40 60:60 25:40 10:40"
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
T=${1:-40}
STEPS=${2:-"10:40 6:40 3:40 0:20 10:40 25:40 60:60 25:40 10:40"}
MOVE=${MOVE:-ue1}
SETTLE=${SETTLE:-5}
case $MOVE in
  ue1) DU=du2 ;;
  ue2) DU=du1 ;;
  *) echo "MOVE must be ue1 or ue2"; exit 1 ;;
esac
L1=$root/results/f1-smoke/ue1/ue1.log
L2=$root/results/f1-smoke/ue2/ue2.log
LM=$root/results/f1-smoke/$MOVE/$MOVE.log

ctrl()   { echo "$*" | sudo ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 4099 >/dev/null; }
sizes()  { tail -n +"$2" "$1" | grep -o 'MeasurementReport Encoded [0-9]* bits' | awk '{print $3}' \
             | sort -n | uniq -c | awk '{printf "%sx%sb ", $1, $2}'; }
count()  { tail -n +"$2" "$1" | grep -c "$3"; }
timing() { tail -n +"$2" "$1" | grep 'neighbour PCI .* confirmed' | grep -o '\(serving-timing\|search\) check pos [0-9]*' \
             | awk '{print ($1=="search" ? "S" : "") $4}' | sort | uniq -c | sort -rn | head -3 \
             | awk '{printf "%sx%s ", $1, $2}'; }
ratios() { local all; all=$(tail -n +"$2" "$1" | grep -c 'check pos ')
           tail -n +"$2" "$1" | grep -o 'check pos -\?[0-9]* ratio [0-9.]* thr [a-z]*' | awk '$3 >= 0' \
             | LC_ALL=C sort -k5,5 -g | LC_ALL=C awk -v all="$all" '{r[NR]=$5; t[$7]++}
                 END {if (NR==0) {printf "no tested timing (%d diag lines)\n", all; exit}
                 printf "median %.2f max %.2f min %.2f (thr:", r[int((NR+1)/2)], r[NR], r[1]
                 for (k in t) printf " %s %d", k, t[k]; printf "; %d of %d diag lines tested a timing)\n", NR, all}'; }
CUSTATS=$root/results/f1-smoke/cu/nrRRC_stats.log
CUID=${MOVE#ue}   # lab-up.sh starts UE1 first: CU UE ID 1 = UE1, 2 = UE2
phy_vals() { local s n
  s=$(tail -n +"$2" "$1" | grep -o 'SSB 0 SINR [0-9.-]* dB RSRP -\?[0-9]* dBm' | tail -n 1 | awk '{print $(NF-1)}')
  n=$(tail -n +"$2" "$1" | grep -o 'confirmed (.*sent -\?[0-9]* dBm' | tail -n 1 | awk '{print $(NF-1)}')
  if [[ -n $s && -n $n ]]; then echo "serving $s neighbour $n gap $((s - n))"; else echo "serving ${s:--} neighbour ${n:--}"; fi; }
cu_vals() { awk -v id="$CUID" '/^UE [0-9]+ CU UE ID/ {u = ($6 == id)} u && /servingCellId/ {w = "s"} u && /neighboring cell/ {w = "n"}
    u && /resultSSB: RSRP/ {v[w] = $3}
    END {if ("s" in v && "n" in v) printf "serving %s neighbour %s gap %d", v["s"], v["n"], v["s"] - v["n"]
         else printf "serving %s neighbour %s", ("s" in v ? v["s"] : "-"), ("n" in v ? v["n"] : "-")}' "$CUSTATS"; }
# Controller (phase 1b): last CSV row of this CU UE ID; columns found by header name. Needs CTRL_CSV=<path>
# (the controller started with E2E_CSV=<same path>). Values are index - 157 (= UE value), so ctrl = CU - 1.
CTRL_CSV=${CTRL_CSV:-}
ctrl_vals() { [[ -n $CTRL_CSV && -r $CTRL_CSV ]] || { echo "-"; return; }
  awk -F, -v id="$CUID" 'NR == 1 {for (i = 1; i <= NF; i++) c[$i] = i; next}
    $c["cu_ue_id"] == id {s = $c["srv_rsrp_dbm"]; sp = $c["srv_pci"]; n = $c["nb_rsrp_dbm"]; np = $c["nb_pci"]
                          nn = $c["n_nb"]; sa = $c["srv_age_ms"]; m += $c["n_meas_cu"]; rows++}
    END {if (!rows) {print "no row"; exit}
         if (s == "") {print "no report"; exit}
         printf "serving %s (PCI %s) ", s, sp
         if (n != "") printf "neighbour %s (PCI %s) gap %d", n, np, s - n; else printf "no neighbour"
         printf ", n_nb %s, age %s ms (whole CSV: %d reports, %d rows)", nn, sa, m, rows}' "$CTRL_CSV"; }
restore() { ctrl loss "$MOVE" "$DU" 10; echo "hub restored: loss $MOVE $DU 10"; }

for f in "$L1" "$L2"; do
  grep -q 'neighbour diag lines on' "$f" || { echo "no diag lines in $f: run E2E_NEIGH_DIAG=1 tools/lab-up.sh first"; exit 1; }
done
sudo -v || exit 1
trap 'restore; exit 1' INT TERM
echo "moving: $MOVE-$DU, settle ${SETTLE} s"
printf '%-9s | %-20s %-4s %-3s %-4s %-5s %-14s | %-20s %-4s %-3s %-4s %-5s %-14s\n' "$MOVE-$DU" \
  "UE1 reports" LOST ADD conf nodet "UE1 timing" "UE2 reports" LOST ADD conf nodet "UE2 timing"
for step in $STEPS; do
  loss=${step%%:*}; secs=$T
  [[ $step == *:* ]] && secs=${step#*:}
  a1=$(wc -l < "$L1"); a2=$(wc -l < "$L2")   # before the change: LOST/ADD can follow within ~0.1 s
  ctrl loss "$MOVE" "$DU" "$loss"
  sleep "$SETTLE"
  n1=$(wc -l < "$L1"); n2=$(wc -l < "$L2"); nm=$(wc -l < "$LM")
  sleep $((secs - SETTLE))
  printf '%-9s | %-20s %-4s %-3s %-4s %-5s %-14s | %-20s %-4s %-3s %-4s %-5s %-14s\n' "$loss dB ${secs}s" \
    "$(sizes "$L1" "$n1")" "$(count "$L1" "$a1" 'LOST after')" "$(count "$L1" "$a1" ' ADDED:')" \
    "$(count "$L1" "$n1" ' confirmed (')" "$(count "$L1" "$n1" 'not detected (')" "$(timing "$L1" "$n1")" \
    "$(sizes "$L2" "$n2")" "$(count "$L2" "$a2" 'LOST after')" "$(count "$L2" "$a2" ' ADDED:')" \
    "$(count "$L2" "$n2" ' confirmed (')" "$(count "$L2" "$n2" 'not detected (')" "$(timing "$L2" "$n2")"
  echo "   $MOVE ratio: $(ratios "$LM" "$nm")"
  echo "   $MOVE dBm: UE PHY $(phy_vals "$LM" "$nm") | CU $(cu_vals)"
  [[ -n $CTRL_CSV ]] && echo "   $MOVE dBm: controller $(ctrl_vals)"
  tail -n +"$nm" "$LM" | grep -E ' confirmed \(| not detected \(' | tail -n 1 | sed "s/^.*\] neighbour/   $MOVE last: neighbour/"
done
restore
