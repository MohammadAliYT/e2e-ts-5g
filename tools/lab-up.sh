#!/usr/bin/env bash
# Start the lab in order: core -> RIC -> CU -> DU1 -> DU2 -> UE1 -> UE2
# Core/webconsole are skipped if already running. RIC runs inside $RAN_NS.
# LAB_MODE=gate:               single-radio gate test, order core -> RIC -> CU -> DU1 -> UE1 -> DU2, no UE2.
#                              UE1 is the RFsim server (one radio); DU1 and DU2 connect to it as clients
#                              (order from upstream/oai/doc/handover-tutorial.md).
# LAB_MODE=gate-samefreq:      like gate, but DU2 on DU1's frequency (intra-frequency neighbours).
# LAB_MODE=gate-samefreq-ssb0: like gate-samefreq, DU2 SSB at index 0, UE1 with a channel model
#                              per DU (DU2 10 dB weaker) and telnet on port 9090. DU2 joins BEFORE
#                              UE1 attaches.
# LAB_MODE=hub:                relay hub (tools/rfsim-hub): every UE has one radio that hears every DU.
#                              Order core -> RIC -> CU -> hub -> DU1 -> DU2 -> UE1 -> UE2.
#   HUB_DUS=1|2, HUB_UES=1|2   number of DUs / UEs (default 2 / 2)
#   HUB_INIT='cmd;cmd'         hub commands sent before the DUs start, e.g. 'loss ue1 du2 10;loss ue2 du1 10'
# UE1_EXTRA='...': extra nr-uesoftmodem options for UE1 in gate and hub modes (e.g. --log_config.nr_rrc_log_level debug)
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
RIC=$root/upstream/oai/openair2/E2AP/flexric/build/examples/ric/nearRT-RIC
WEB=$root/upstream/free5gc/webconsole
HUB=$root/tools/rfsim-hub/rfsim-hub
L=$root/results/lab; mkdir -p "$L"
mode=${LAB_MODE:-normal}
gate_port=4043   # must match RAN/run-node.sh and UE/run-ue.sh
hub_ctrl=4099
case "$mode" in
  normal|gate|gate-samefreq|gate-samefreq-ssb0|hub) ;;
  *) echo "Unknown LAB_MODE: $mode (normal|gate|gate-samefreq|gate-samefreq-ssb0|hub)"; exit 1 ;;
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
# number of established connections on local port $2 in namespace $1, at least $3
conn_test() { echo "[ \$(sudo ip netns exec $1 ss -Htn state established '( sport = :$2 )' | wc -l) -ge $3 ]"; }
ue_ip_test() { echo "sudo ip netns exec $1 ip -4 -o addr show dev $2 | grep -q inet"; }

ip netns list | grep -q "^$RAN_NS" || { echo "namespaces missing - run RAN/interfaces/netns.sh up"; exit 1; }
pgrep -f 'nr-(ue)?softmodem' >/dev/null && { echo "RAN already running - run tools/lab-down.sh first"; exit 1; }
pgrep -f 'rfsim-[h]ub' >/dev/null && { echo "hub already running - run tools/lab-down.sh first"; exit 1; }
[[ $mode == hub && ! -x $HUB ]] && { echo "hub not built: $HUB"; exit 1; }

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
  wait_for "UE1 has IP" 60 "$(ue_ip_test "$UE1_NS" ue1)"
  bg ue2 UE/run-ue.sh ue2 ${UE2_VARIANT:+"$UE2_VARIANT"}
  wait_for "UE2 has IP" 60 "$(ue_ip_test "$UE2_NS" ue2)"
  ues=(1 2)
elif [[ $mode == hub ]]; then
  ndu=${HUB_DUS:-2}; nue=${HUB_UES:-2}
  hub_args=()
  for ((j=1; j<=ndu; j++)); do hub_args+=(--du $((4042+j))); done
  for ((k=1; k<=nue; k++)); do hub_args+=(--ue $((4050+k))); done
  echo "== hub ($ndu DU, $nue UE)"
  bg hub sudo ip netns exec "$RAN_NS" stdbuf -oL "$HUB" "${hub_args[@]}" --ctrl "$hub_ctrl"
  wait_for "hub (control port $hub_ctrl)" 10 "sudo ip netns exec $RAN_NS ss -Hltn | grep -q ':$hub_ctrl '"
  if [[ -n ${HUB_INIT:-} ]]; then
    IFS=';' read -r -a cmds <<<"$HUB_INIT"
    for c in "${cmds[@]}"; do
      printf '%s\n' "$c" | sudo ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 "$hub_ctrl" | sed 's/^/  hub: /'
    done
  fi
  for ((j=1; j<=ndu; j++)); do
    bg du$j RAN/run-node.sh du$j hub
    wait_for "DU$j connected to hub" 40 "$(conn_test "$RAN_NS" $((4042+j)) 1)"
  done
  echo "== UEs via hub (one after another -> CU UE IDs 1 and 2)"
  bg ue1 UE/run-ue.sh ue1 hub ${UE1_EXTRA:-}   # UE1_EXTRA unquoted: split into words
  wait_for "UE1 has IP" 90 "$(ue_ip_test "$UE1_NS" ue1)"
  ues=(1)
  if ((nue >= 2)); then
    bg ue2 UE/run-ue.sh ue2 hub
    wait_for "UE2 has IP" 90 "$(ue_ip_test "$UE2_NS" ue2)"
    ues=(1 2)
  fi
else
  # DU1 is an RFsim client; it retries "connect() to ... failed" until the UE1 server is up
  bg du1 RAN/run-node.sh du1 "$var"
  wait_for "DU1 started, connecting to UE1" 40 "grep -q 'connect() to' '$L/du1.out'"
  echo "== UE1 (RFsim server, one radio, variant $ue_var)"
  bg ue1 UE/run-ue.sh ue1 "$ue_var" ${UE1_EXTRA:-}   # UE1_EXTRA unquoted: split into words
  if [[ $mode == gate-samefreq-ssb0 ]]; then
    wait_for "DU1 connected to UE1" 40 "$(conn_test "$UE1_NS" $gate_port 1)"
    bg du2 RAN/run-node.sh du2 "$var"
    wait_for "DU2 connected to UE1" 40 "$(conn_test "$UE1_NS" $gate_port 2)"
    wait_for "UE1 has IP (via DU1)" 90 "$(ue_ip_test "$UE1_NS" ue1)"
  else
    # tutorial order: DU2 only after UE1 is attached (no channel emulation, both DUs equally strong)
    wait_for "UE1 has IP (via DU1)" 90 "$(ue_ip_test "$UE1_NS" ue1)"
    bg du2 RAN/run-node.sh du2 "$var"
    wait_for "DU2 connected to UE1" 40 "$(conn_test "$UE1_NS" $gate_port 2)"
  fi
  ues=(1)
fi

echo "== checks"
[[ -f $L/ric.out ]] && echo "  E2 setups at RIC: $(grep -c 'E2 SETUP-REQUEST rx' "$L/ric.out")"
for u in "${ues[@]}"; do ns=UE${u}_NS
  sudo ip netns exec "${!ns}" ping -c2 -W2 -I ue$u "$DN_IP" >/dev/null && echo "  ok    UE$u -> DN" || echo "  FAIL  UE$u -> DN"; done
echo "lab up ($mode). Logs: results/f1-smoke/<node>/, results/core/, results/lab/"
