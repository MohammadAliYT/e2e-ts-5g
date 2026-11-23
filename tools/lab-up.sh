#!/usr/bin/env bash
# Start the lab (hub mode) in order: core -> RIC -> CU -> hub -> DU1 -> DU2 -> UE1 -> UE2
# Core/webconsole are skipped if already running. RIC, CU, DUs and hub run inside $RAN_NS.
# The hub (tools/rfsim-hub) is the only RFsim server: every UE has one radio that hears every DU.
#   HUB_DUS=1|2, HUB_UES=1|2   number of DUs / UEs (default 2 / 2)
#   HUB_NOISE=<dBFS>|off       receiver noise floor on every hub node (default -75: cell centre MCS 28,
#                              MCS 0 at ~20 dB path loss, link degraded at ~25 dB; results/hub-tel/)
#   HUB_INIT='cmd;cmd'         hub commands sent before the DUs start
#                              (default 'loss ue1 du2 10;loss ue2 du1 10': UE1 near DU1, UE2 near DU2)
#   UE1_EXTRA='...'            extra nr-uesoftmodem options for UE1 (e.g. --log_config.nr_rrc_log_level debug)
#   E2E_NEIGH_DIAG=1           both UEs log the neighbour-measurement diag lines (needed by tools/neigh-meas-test.sh;
#                              default 0 = off; oai-ue-neigh-known-pci.patch)
#   A3_MODE=ctrl|set1|set5     CU A3 setting (default ctrl: 15 dB, no automatic handover; set1: 3 dB / 480 ms,
#                              set5: -1 dB / 40 ms, TR 36.839 sets, the CU hands over on A3; RAN/run-node.sh)
# The old modes (normal, gate, gate-samefreq, gate-samefreq-ssb0) are in the git history.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
RIC=$root/upstream/oai/openair2/E2AP/flexric/build/examples/ric/nearRT-RIC
WEB=$root/upstream/free5gc/webconsole
HUB=$root/tools/rfsim-hub/rfsim-hub
L=$root/results/lab; mkdir -p "$L"
hub_ctrl=4099
if [[ -n ${LAB_MODE:-} && $LAB_MODE != hub ]]; then
  echo "LAB_MODE=$LAB_MODE: only hub mode exists now (old modes: git history)"; exit 1
fi
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
[[ -x $HUB ]] || { echo "hub not built: $HUB"; exit 1; }

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

echo "== RAN (hub mode)"
bg cu env A3_MODE="${A3_MODE:-ctrl}" RAN/run-node.sh cu   # sudo drops the variable, so pass it on
wait_for "CU (F1-C 38472)" 30 "sudo ip netns exec $RAN_NS ss -HlnS | grep -q ':38472'"

ndu=${HUB_DUS:-2}; nue=${HUB_UES:-2}
hub_args=()
for ((j=1; j<=ndu; j++)); do hub_args+=(--du $((4042+j))); done
for ((k=1; k<=nue; k++)); do hub_args+=(--ue $((4050+k))); done
echo "== hub ($ndu DU, $nue UE)"
bg hub sudo ip netns exec "$RAN_NS" stdbuf -oL "$HUB" "${hub_args[@]}" --ctrl "$hub_ctrl"
wait_for "hub (control port $hub_ctrl)" 10 "sudo ip netns exec $RAN_NS ss -Hltn | grep -q ':$hub_ctrl '"
hub_cmds=()
if [[ ${HUB_NOISE:--75} != off ]]; then
  for ((j=1; j<=ndu; j++)); do hub_cmds+=("noise du$j ${HUB_NOISE:--75}"); done
  for ((k=1; k<=nue; k++)); do hub_cmds+=("noise ue$k ${HUB_NOISE:--75}"); done
fi
IFS=';' read -r -a cmds <<<"${HUB_INIT-loss ue1 du2 10;loss ue2 du1 10}"
hub_cmds+=("${cmds[@]}")
for c in "${hub_cmds[@]}"; do
  [[ -n $c ]] || continue
  printf '%s\n' "$c" | sudo ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 "$hub_ctrl" | sed "s/^/  hub: $c -> /"
done
for ((j=1; j<=ndu; j++)); do
  bg du$j RAN/run-node.sh du$j
  wait_for "DU$j connected to hub" 40 "$(conn_test "$RAN_NS" $((4042+j)) 1)"
done

echo "== UEs via hub (one after another -> CU UE IDs 1 and 2)"
bg ue1 env E2E_NEIGH_DIAG="${E2E_NEIGH_DIAG:-0}" UE/run-ue.sh ue1 ${UE1_EXTRA:-}   # UE1_EXTRA unquoted: split into words
wait_for "UE1 has IP" 90 "$(ue_ip_test "$UE1_NS" ue1)"
ues=(1)
if ((nue >= 2)); then
  bg ue2 env E2E_NEIGH_DIAG="${E2E_NEIGH_DIAG:-0}" UE/run-ue.sh ue2
  wait_for "UE2 has IP" 90 "$(ue_ip_test "$UE2_NS" ue2)"
  ues=(1 2)
fi

echo "== checks"
[[ -f $L/ric.out ]] && echo "  E2 setups at RIC: $(grep -c 'E2 SETUP-REQUEST rx' "$L/ric.out")"
for u in "${ues[@]}"; do ns=UE${u}_NS
  sudo ip netns exec "${!ns}" ping -c2 -W2 -I ue$u "$DN_IP" >/dev/null && echo "  ok    UE$u -> DN" || echo "  FAIL  UE$u -> DN"; done
echo "lab up (hub). Logs: results/f1-smoke/<node>/, results/core/, results/lab/"