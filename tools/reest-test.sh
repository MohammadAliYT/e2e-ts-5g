#!/usr/bin/env bash
# F1 check: what the controller sees after an RRC re-establishment of UE1 on the same DU (DU1).
# Starts the lab (UE1 on DU1, UE2 on DU2) and the controller, sends DL and UL traffic for UE1, then forces a
# re-establishment with the OAI CI telnet command "ci force_reestab" on DU1. It resets the RLC of SRB1 and DRB1;
# the UE detects radio link failure and re-establishes (nr_rlc_test_trigger_reestablishment()).
# Needs: DU telnet server (RAN/run-node.sh, port 9090 on the DU F1 address).
# Usage: sudo -v && (sudo setsid nohup tools/reest-test.sh > /tmp/reest-run.log 2>&1 < /dev/null &)
# Result: results/reest/run-<time>/: run.log, ctrl.csv, ctrl.log, du1.txt, ue1.txt, rows.txt
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
ctrl="$root/upstream/oai/openair2/E2AP/flexric/build/examples/xApp/c/e2e_ctrl/e2e_ctrl"
user=${SUDO_USER:-root}
[[ $EUID -eq 0 ]] || { echo "start the script with sudo (see usage)"; exit 1; }
[[ -x $ctrl ]] || { echo "controller not built: $ctrl"; exit 1; }
pgrep -x e2e_ctrl >/dev/null && { echo "e2e_ctrl runs already - stop it first"; exit 1; }

run="$root/results/reest/run-$(date +%Y%m%d-%H%M%S)"; mkdir -p "$run"
exec > >(tee "$run/run.log") 2>&1
du1tel() { echo "$*" | ip netns exec "$RAN_NS" nc -q 2 "$F1_DU1_IP" 9090 | tr -d '\r' | grep -v '^\s*$'; }

echo "== lab"
tools/lab-down.sh
HUB_DUS=2 HUB_UES=2 tools/lab-up.sh || exit 1
sleep 5

echo "== controller"
E2E_CSV="$run/ctrl.csv" XAPP_DURATION=600 ip netns exec "$RAN_NS" "$ctrl" >"$run/ctrl.log" 2>&1 &
ctrl_pid=$!
sleep 5
kill -0 "$ctrl_pid" 2>/dev/null || { echo "controller stopped - see $run/ctrl.log"; exit 1; }

echo "== traffic UE1: DL 10M + UL 2M for 80 s"
tools/traffic.sh ue1 dl 10M 80 >"$run/ue1-dl.txt" 2>&1 & t1=$!
tools/traffic.sh ue1 ul 2M 80 >"$run/ue1-ul.txt" 2>&1 & t2=$!
sleep 20
echo "== DU1 before"; du1tel ci get_single_rnti
if [[ ${REEST_MODE:-force} == hofail ]]; then
  g=${HO_FAIL_GAP:-20}
  echo "== $(date +%T) handover of UE1 to DU2 with DU2 $g dB weaker (expected: T304 expiry, re-establishment)"
  echo "loss ue1 du2 $g" | ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 4099
  tools/ho-ctrl.sh 1
else
  echo "== $(date +%T) force re-establishment of UE1 on DU1"; du1tel ci force_reestab
fi
sleep 20
echo "== DU1 after"; du1tel ci get_single_rnti
wait "$t1" "$t2"
kill -INT "$ctrl_pid"; wait "$ctrl_pid" 2>/dev/null

echo "== traffic result"; cat "$run/ue1-dl.txt" "$run/ue1-ul.txt"
for d in du1 du2; do
  grep -E "Update old UE ID|Re-establishment detected|Re-establishing RLC|old gNB-DU UE F1AP ID|Remove UE|Added drb" \
    "results/f1-smoke/$d/$d.log" >"$run/$d.txt"
done
grep -E "RRCReestablishment|[Rr]adio link failure|RLF|T304|T310|T311|reconfigurationWithSync" \
  results/f1-smoke/ue1/ue1.log >"$run/ue1.txt"
grep -E "RRCReestablishment|[Rr]adio link failure|RLF|T310|T311" results/f1-smoke/ue1/ue1.log >"$run/ue1.txt"
echo "== DU1 log";  cat "$run/du1.txt"
echo "== DU2 log";  cat "$run/du2.txt"
echo "== UE1 log";  head -20 "$run/ue1.txt"
echo "== controller UE entries"; grep -E "new UE RNTI|gone" "$run/ctrl.log"
python3 -I - "$run/ctrl.csv" >"$run/rows.txt" <<'EOF'
import csv, sys
cols = ["t_rel_s", "du", "rnti", "cu_ue_id", "dl_mbps", "ul_mbps", "dl_mcs", "n_mac", "n_rlc", "rlc_resets",
        "dl_hol_us_mean", "dl_buf_bytes_mean", "kpm_dl_thp_kbps"]
print(" ".join(f"{c:>10}" for c in cols))
for r in csv.DictReader(open(sys.argv[1])):
    if r["cu_ue_id"] in ("1", ""):
        print(" ".join(f"{r[c]:>10}" for c in cols))
EOF
echo "== controller rows, UE1 on both DUs (also in rows.txt)."; cat "$run/rows.txt"
chown -R "$user": "$run" "$root/results/traffic"
echo "done: $run"