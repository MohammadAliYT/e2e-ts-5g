#!/usr/bin/env bash
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
    ip=$F1_DU1_IP; port=$DU1_RFSIM_PORT
    if [[ "$node" == du2 ]]; then ip=$F1_DU2_IP; port=$DU2_RFSIM_PORT; fi
    args=(
      --rfsim
      '--MACRLCs.[0].local_n_address' "$ip"
      '--MACRLCs.[0].remote_n_address' "$F1_CU_IP"
      # --rfsimulator.serverport "$port"
      --rfsimulator.[0].serverport "$port"
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
