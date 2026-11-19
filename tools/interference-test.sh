#!/usr/bin/env bash
# Cross-cell interference control run: does a UE's MCS drop because the OTHER cell is loaded?
# Lab must be up (tools/lab-up.sh) with the default start positions (UE1 near DU1, UE2 near DU2).
# Four cases, DL traffic only, 30 s each; e2e_ctrl writes one CSV per case:
#   A  UE1 5 Mbit/s DL only                (DU2 idle)
#   B  UE2 3 Mbit/s DL only                (DU1 idle)
#   C  both                                (other cell 10 dB weaker = lab default)
#   D  both, other cell 20 dB weaker       (more isolation between the cells)
# Hypothesis: A and B stay near MCS 28; C drops (the other DU's PDSCH arrives only 10 dB below the
# serving DU, same frequency, same slots); D recovers at least partly.
# Output: results/interference/run-<time>/<case>.csv, <case>.log, summary.txt
#         (mean over t_rel >= 10 s, i.e. after the MCS ramp-up; idle UEs show MCS 0)
# Usage: tools/interference-test.sh
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
export LC_NUMERIC=C
CTRL=upstream/oai/openair2/E2AP/flexric/build/examples/xApp/c/e2e_ctrl/e2e_ctrl
out="results/interference/run-$(date +%Y%m%d-%H%M%S)"; mkdir -p "$out"
sudo -v || exit 1
[[ -x $CTRL ]] || { echo "controller not built: $CTRL"; exit 1; }

hub() { printf '%s\n' "$*" | sudo ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 4099 >/dev/null; }

# run_case <name> <flow>...   flow = ue:dir:rate, e.g. ue1:dl:5M
run_case() {
  local name=$1; shift
  echo "== case $name: $*"
  for f in "$@"; do
    IFS=: read -r u d r <<<"$f"
    tools/traffic.sh "$u" "$d" "$r" 30 >/dev/null 2>&1 &
  done
  sudo E2E_CSV="$root/$out/$name.csv" XAPP_DURATION=25 ip netns exec "$RAN_NS" "$root/$CTRL" >"$out/$name.log" 2>&1
  wait
  sleep 3
}

# mean per DU over t_rel >= 10 s (CSV columns: 2 t_rel, 3 du, 8 dl_mbps, 10 dl_mcs, 12 dl_bler)
summarize() {
  awk -F, -v c="$1" 'NR>1 && $2>=10 { n[$3]++; t[$3]+=$8; m[$3]+=$10; b[$3]+=$12 }
    END { for (d in n) printf "%s  DU%s  dl %.2f Mbit/s  dl_mcs %4.1f  dl_bler %.3f  (%d s)\n", c, d, t[d]/n[d], m[d]/n[d], b[d]/n[d], n[d] }' \
    "$out/$1.csv" | sort
}

run_case A ue1:dl:5M
run_case B ue2:dl:3M
run_case C ue1:dl:5M ue2:dl:3M
hub loss ue1 du2 20; hub loss ue2 du1 20; sleep 5
run_case D ue1:dl:5M ue2:dl:3M
hub loss ue1 du2 10; hub loss ue2 du1 10   # back to the lab default

{ echo "# $(date -Is) interference test (A: UE1 only, B: UE2 only, C: both 10 dB, D: both 20 dB)"
  for c in A B C D; do summarize "$c"; done; } | tee "$out/summary.txt"
echo "results: $out"