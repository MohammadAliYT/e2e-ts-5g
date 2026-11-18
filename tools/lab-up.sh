#!/usr/bin/env bash
# Start the lab in order: core -> RIC -> CU -> DU1 -> DU2 -> UE1 -> UE2
# Core/webconsole are skipped if already running. RIC runs inside $RAN_NS.
# LAB_MODE=gate:               single-radio gate test, order core -> RIC -> CU -> DU1 -> UE1 -> DU2, no UE2.
#                              UE1 is the RFsim server (one radio); DU1 and DU2 connect to it as clients
#                              (order from upstream/oai/doc/handover-tutorial.md).
# LAB_MODE=gate-samefreq:      like gate, but DU2 on DU1's frequency (intra-frequency neighbours).
# LAB_MODE=gate-samefreq-ssb0: like gate-samefreq, DU2 SSB at index 0, UE1 with a channel model
#                              per DU (DU2 10 dB weaker) and telnet on port 9090. DU2 joins BEFORE
#                              UE1 attaches, so the CU lists DU2 in UE1's first measurement config.
# UE1_EXTRA='...': extra nr-uesoftmodem options for UE1 in gate modes (e.g. --log_config.nr_rrc_log_level debug)
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
RIC=$root/upstream/oai/openair2/E2AP/flexric/build/examples/ric/nearRT-RIC
WEB=$root/upstream/free5gc/webconsole
L=$root/results/lab; mkdir -p "$L"
mode=${LAB_MODE:-normal}
gate_port=4043   # must match RAN/run-node.sh and UE/run-ue.sh
case "$mode" in
  normal|gate|gate-samefreq|gate-samefreq-ssb0) ;;
  *) echo "Unknown LAB_MODE: $mode (normal|gate|gate-samefreq|gate-samefreq-ssb0)"; exit 1 ;;
esac
var=""; [[ $mode != normal ]] && var=$mode              # variant passed to run-node.sh (cu, du1, du2)
ue_var=gate; [[ $mode == gate-samefreq-ssb0 ]] && ue_var=gate-chanmod   # variant for UE1 in gate modes
sudo -v || exit 1

# setsid: the process survives when lab-up.sh exits (no SIGHUP)
bg()  { local n=$1; shift; sudo HOME="$HOME" setsid -f nohup "$@" >"$L/$n.out" 2>&1 </dev/null; echo "  start $n"; }
bgu() { local n=$1; shift; nohup "$@" >"$L/$n.out" 2>&1 </dev/null & echo "  start $n"; }
wait_for() { # <label> <timeout s> <test>
  for ((i=0; i<$2; i++)); do bash -c "$3" >/dev/null 2>&1 && { echo "  ok    $1"; return 0; }; sleep 1; done
  echo "  FAIL  $1 - see $L/"; exit 1; }
inran() { sudo ip netns exec "$RAN_NS" "$@"; }
# number of DUs connected to UE1's RFsim server
conn_test() { echo "[ \$(sudo ip netns exec $UE1_NS ss -Htn state established '( sport = :$gate_port )' | wc -l) -ge $1 ]"; }

ip netns list | grep -q "^$RAN_NS" || { echo "namespaces missing - run RAN/interfaces/netns.sh up"; exit 1; }
pgrep -f 'nr-(ue)?softmodem' >/dev/null && { echo "RAN already running - run tools/lab-down.sh first"; exit 1; }

echo "== core"
if ss -HlnS | grep -q ':38412'; then echo "  core already running"; else
  bgu core Core/run-core.sh
  wait_for "AMF (NGAP 38412)" 60 "ss -HlnS | grep -q ':38412'"
  sleep 3; fi
if ss -Hltn | grep -q ':5000 '; then echo "  webconsole already running"; else
  (cd "$WEB" && bgu webconsole ./bin/webconsole); fi

echo "== RIC (in $RAN_NS)"
if inran ss -HlnS | grep -q ':36421'; then echo "  RIC already running"; else
  pgrep -f '[n]earRT-RIC' >/dev/null && { echo "RIC running outside $RAN_NS - run tools/lab-down.sh"; exit 1; }
  # stdbuf: unbuffered RIC output
  bg ric sudo ip netns exec "$RAN_NS" stdbuf -oL -eL "$RIC"
  wait_for "RIC (E2 36421)" 20 "sudo ip netns exec $RAN_NS ss -HlnS | grep -q ':36421'"; fi

echo "== RAN (mode: $mode)"
bg cu RAN/run-node.sh cu ${var:+"$var"}
wait_for "CU (F1-C 38472)" 30 "sudo ip netns exec $RAN_NS ss -HlnS | grep -q ':38472'"

if [[ $mode == normal ]]; then
  bg du1 RAN/run-node.sh du1
  wait_for "DU1 (RFsim $DU1_RFSIM_PORT)" 40 "sudo ip netns exec $RAN_NS ss -Hltn | grep -q ':$DU1_RFSIM_PORT '"
  bg du2 RAN/run-node.sh du2
  wait_for "DU2 (RFsim $DU2_RFSIM_PORT)" 40 "sudo ip netns exec $RAN_NS ss -Hltn | grep -q ':$DU2_RFSIM_PORT '"
  echo "== UEs (one after another -> CU UE IDs 1 and 2)"
  bg ue1 UE/run-ue.sh ue1 ${UE1_VARIANT:+"$UE1_VARIANT"}
  wait_for "UE1 has IP" 60 "sudo ip netns exec $UE1_NS ip -4 -o addr show dev ue1 | grep -q inet"
  bg ue2 UE/run-ue.sh ue2 ${UE2_VARIANT:+"$UE2_VARIANT"}
  wait_for "UE2 has IP" 60 "sudo ip netns exec $UE2_NS ip -4 -o addr show dev ue2 | grep -q inet"
  ues=(1 2)
else
  # DU1 is an RFsim client; it retries "connect() to ... failed" until the UE1 server is up
  bg du1 RAN/run-node.sh du1 "$var"
  wait_for "DU1 started, connecting to UE1" 40 "grep -q 'connect() to' '$L/du1.out'"
  echo "== UE1 (RFsim server, one radio, variant $ue_var)"
  bg ue1 UE/run-ue.sh ue1 "$ue_var" ${UE1_EXTRA:-}   # UE1_EXTRA unquoted: split into words
  if [[ $mode == gate-samefreq-ssb0 ]]; then
    # DU2 is 10 dB weaker (channel model), so it may join before UE1 attaches: the CU then
    # knows DU2 when it sends UE1's measurement config. DU2 starts after DU1 is connected,
    # so the model order stays ue0 = DU1, ue1 = DU2.
    wait_for "DU1 connected to UE1" 40 "$(conn_test 1)"
    bg du2 RAN/run-node.sh du2 "$var"
    wait_for "DU2 connected to UE1" 40 "$(conn_test 2)"
    wait_for "UE1 has IP (via DU1)" 90 "sudo ip netns exec $UE1_NS ip -4 -o addr show dev ue1 | grep -q inet"
  else
    # tutorial order: DU2 only after UE1 is attached (no channel emulation, both DUs equally strong)
    wait_for "UE1 has IP (via DU1)" 90 "sudo ip netns exec $UE1_NS ip -4 -o addr show dev ue1 | grep -q inet"
    bg du2 RAN/run-node.sh du2 "$var"
    wait_for "DU2 connected to UE1" 40 "$(conn_test 2)"
  fi
  ues=(1)
fi

echo "== checks"
[[ -f $L/ric.out ]] && echo "  E2 setups at RIC: $(grep -c 'E2 SETUP-REQUEST rx' "$L/ric.out") (expect 3)"
for u in "${ues[@]}"; do ns=UE${u}_NS
  sudo ip netns exec "${!ns}" ping -c2 -W2 -I ue$u "$DN_IP" >/dev/null && echo "  ok    UE$u -> DN" || echo "  FAIL  UE$u -> DN"; done
echo "lab up ($mode). Logs: results/f1-smoke/<node>/, results/core/, results/lab/"