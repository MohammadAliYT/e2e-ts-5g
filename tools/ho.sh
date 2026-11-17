#!/usr/bin/env bash
# E2 handover of one UE to the other DU.  Usage: tools/ho.sh <CU-UE-ID>
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); source "$root/RAN/interfaces/lab.env"
X=$root/upstream/oai/openair2/E2AP/flexric/build/examples/xApp/c/rc_handover/xapp_rc_handover
S=$root/results/f1-smoke; O=$root/results/lab/ho-last.out; mkdir -p "$root/results/lab"
[[ $# -eq 1 ]] || { echo "usage: $0 <CU-UE-ID>"; exit 1; }
serving() { for d in du1 du2; do echo "  $d: $(grep -o 'CU-UE-ID [0-9]*' "$S/$d/nrMAC_stats.log" | tr '\n' ' ')"; done; }
echo "before:"; serving
sudo ip netns exec "$RAN_NS" env HO_CU_UE_ID="$1" timeout -s KILL 20 "$X" >"$O" 2>&1
[[ $? -eq 137 ]] && echo "xApp timed out (RIC/E2 problem?) - see $O"
grep -E "Selected UE|not found" "$O"
sleep 3; echo "after:"; serving