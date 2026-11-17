#!/usr/bin/env bash
# Start the lab in order: core -> RIC -> CU -> DU1 -> DU2 -> UE1 -> UE2
# Core/webconsole are skipped if already running. RIC runs inside $RAN_NS.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
RIC=$root/upstream/oai/openair2/E2AP/flexric/build/examples/ric/nearRT-RIC
WEB=$root/upstream/free5gc/webconsole
L=$root/results/lab; mkdir -p "$L"
sudo -v || exit 1

# bg() { local n=$1; shift; nohup "$@" >"$L/$n.out" 2>&1 </dev/null & echo "  start $n"; }

# claude suggestion: use setsid to avoid SIGHUP killing the process when lab-up.sh exits
bg()  { local n=$1; shift; sudo HOME="$HOME" setsid -f nohup "$@" >"$L/$n.out" 2>&1 </dev/null; echo "  start $n"; }
bgu() { local n=$1; shift; nohup "$@" >"$L/$n.out" 2>&1 </dev/null & echo "  start $n"; }
wait_for() { # <label> <timeout s> <test>
  for ((i=0; i<$2; i++)); do bash -c "$3" >/dev/null 2>&1 && { echo "  ok    $1"; return 0; }; sleep 1; done
  echo "  FAIL  $1 - see $L/"; exit 1; }
inran() { sudo ip netns exec "$RAN_NS" "$@"; }

ip netns list | grep -q "^$RAN_NS" || { echo "namespaces missing - run RAN/interfaces/netns.sh up"; exit 1; }
pgrep -f 'nr-(ue)?softmodem' >/dev/null && { echo "RAN already running - run tools/lab-down.sh first"; exit 1; }

echo "== core"
if ss -HlnS | grep -q ':38412'; then echo "  core already running"; else
  # bg core Core/run-core.sh
  bgu core Core/run-core.sh
  wait_for "AMF (NGAP 38412)" 60 "ss -HlnS | grep -q ':38412'"
  sleep 3; fi
if ss -Hltn | grep -q ':5000 '; then echo "  webconsole already running"; else
  # (cd "$WEB" && bg webconsole ./bin/webconsole); fi
  (cd "$WEB" && bgu webconsole ./bin/webconsole); fi
echo "== RIC (in $RAN_NS)"
if inran ss -HlnS | grep -q ':36421'; then echo "  RIC already running"; else
  pgrep -f '[n]earRT-RIC' >/dev/null && { echo "RIC running outside $RAN_NS - run tools/lab-down.sh"; exit 1; }
  # bg ric sudo ip netns exec "$RAN_NS" "$RIC"
  # claude suggestion: use stdbuf to avoid buffering issues with the RIC output
  bg ric sudo ip netns exec "$RAN_NS" stdbuf -oL -eL "$RIC"
  wait_for "RIC (E2 36421)" 20 "sudo ip netns exec $RAN_NS ss -HlnS | grep -q ':36421'"; fi

echo "== RAN"
bg cu RAN/run-node.sh cu
wait_for "CU (F1-C 38472)" 30 "sudo ip netns exec $RAN_NS ss -HlnS | grep -q ':38472'"
bg du1 RAN/run-node.sh du1
wait_for "DU1 (RFsim $DU1_RFSIM_PORT)" 40 "sudo ip netns exec $RAN_NS ss -Hltn | grep -q ':$DU1_RFSIM_PORT '"
bg du2 RAN/run-node.sh du2
wait_for "DU2 (RFsim $DU2_RFSIM_PORT)" 40 "sudo ip netns exec $RAN_NS ss -Hltn | grep -q ':$DU2_RFSIM_PORT '"

echo "== UEs (one after another -> CU UE IDs 1 and 2)"
bg ue1 UE/run-ue.sh ue1
wait_for "UE1 has IP" 60 "sudo ip netns exec $UE1_NS ip -4 -o addr show dev ue1 | grep -q inet"
bg ue2 UE/run-ue.sh ue2
wait_for "UE2 has IP" 60 "sudo ip netns exec $UE2_NS ip -4 -o addr show dev ue2 | grep -q inet"

echo "== checks"
[[ -f $L/ric.out ]] && echo "  E2 setups at RIC: $(grep -c 'E2 SETUP-REQUEST rx' "$L/ric.out") (expect 3)"
for u in 1 2; do ns=UE${u}_NS
  sudo ip netns exec "${!ns}" ping -c2 -W2 -I ue$u "$DN_IP" >/dev/null && echo "  ok    UE$u -> DN" || echo "  FAIL  UE$u -> DN"; done
echo "lab up. Logs: results/f1-smoke/<node>/, results/core/, results/lab/"