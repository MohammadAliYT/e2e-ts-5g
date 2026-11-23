#!/usr/bin/env bash
# Start the CU (central unit) or a DU (distributed unit) in the RAN namespace, in hub mode.
# Usage: RAN/run-node.sh <cu|du1|du2>
#   CU:  RAN/cu/cu.conf (includes neighbour.conf: both DUs on the same frequency).
#   DU:  RAN/du/<du>.conf, RFsim (RF simulator) client of tools/rfsim-hub, which runs in the
#        same namespace: DU1 -> port 4043, DU2 -> port 4044.
#   WITH_E2=1: append an e2_agent section to a copy of the config (runtime.conf).
# The old modes (normal, gate, gate-samefreq, gate-samefreq-ssb0) are in the git history.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"

node="${1:?Pass cu, du1, or du2}"

case "$node" in
  cu)
    config=cu/cu.conf
    args=(
      '--gNBs.[0].local_s_address' "$F1_CU_IP"
      '--gNBs.[0].amf_ip_address.[0].ipv4' "$AMF_IP"
      '--gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NG_AMF' "$CU_N2_N3_CIDR"
      '--gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NGU' "$CU_N2_N3_CIDR"
      --telnetsrv --telnetsrv.shrmod ci --telnetsrv.listenaddr "$F1_CU_IP"
    )
    ;;
  du1|du2)
    config="du/$node.conf"
    ip=$F1_DU1_IP; hub_port=4043
    if [[ "$node" == du2 ]]; then ip=$F1_DU2_IP; hub_port=4044; fi
    args=(
      --rfsim
      '--MACRLCs.[0].local_n_address' "$ip"
      '--MACRLCs.[0].remote_n_address' "$F1_CU_IP"
      '--rfsimulator.[0].serveraddr' 127.0.0.1
      '--rfsimulator.[0].serverport' "$hub_port"
      --telnetsrv --telnetsrv.shrmod ci --telnetsrv.listenaddr "$ip"   # OAI CI commands (port 9090), e.g. ci force_reestab
    )
    ;;
  *) printf 'Unknown node: %s\n' "$node" >&2; exit 1 ;;
esac

work="$root/results/f1-smoke/$node"
mkdir -p "$work"
cd "$work"
config="$root/RAN/$config"

if [[ "${WITH_E2:-0}" == 1 ]]; then
  cp "$config" runtime.conf
  printf '\ne2_agent = { near_ric_ip_addr = "%s"; sm_dir = "/usr/local/lib/flexric/"; };\n' \
    "$E2_RIC_IP" >> runtime.conf
  config="$work/runtime.conf"
fi

sudo ip netns exec "$RAN_NS" \
  "$root/upstream/oai/cmake_targets/ran_build/build/nr-softmodem" \
  -O "$config" "${args[@]}" 2>&1 | tee "$node.log"