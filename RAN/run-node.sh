#!/usr/bin/env bash
# Start the CU or a DU in the RAN namespace.
# Usage: run-node.sh <cu|du1|du2> [gate|gate-samefreq|gate-samefreq-ssb0|hub]
#   gate:               the DU is an RFsim CLIENT of UE1 (UE1 is the RFsim server), single-radio gate test.
#   gate-samefreq:      like gate, plus DU2 on DU1's frequency and the matching CU neighbour config.
#   gate-samefreq-ssb0: like gate-samefreq, plus DU2 sends its SSB at index 0 (same as DU1).
#   hub:                the DU is an RFsim CLIENT of tools/rfsim-hub (DU1 -> port 4043, DU2 -> 4044);
#                       both DUs on 630048 with SSB index 0, CU with same-frequency neighbours.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"

node="${1:?Pass cu, du1, or du2}"
variant="${2:-}"
gate_port=4043   # RFsim port of UE1 in gate mode

case "$node" in
  cu)
    config=cu/cu.conf
    case "$variant" in
      ""|gate) ;;
      gate-samefreq|gate-samefreq-ssb0|hub) config=cu/cu.samefreq.conf ;;
      *) printf 'Unknown variant: %s\n' "$variant" >&2; exit 1 ;;
    esac
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
    ip=$F1_DU1_IP; port=$DU1_RFSIM_PORT; hub_port=4043
    if [[ "$node" == du2 ]]; then ip=$F1_DU2_IP; port=$DU2_RFSIM_PORT; hub_port=4044; fi
    args=(
      --rfsim
      '--MACRLCs.[0].local_n_address' "$ip"
      '--MACRLCs.[0].remote_n_address' "$F1_CU_IP"
    )
    case "$variant" in
      "") args+=('--rfsimulator.[0].serverport' "$port") ;;
      gate|gate-samefreq|gate-samefreq-ssb0)
        args+=('--rfsimulator.[0].serveraddr' "$UE1_IP" '--rfsimulator.[0].serverport' "$gate_port")
        if [[ $node == du2 && $variant == gate-samefreq ]];      then config=du/du2.samefreq.conf; fi
        if [[ $node == du2 && $variant == gate-samefreq-ssb0 ]]; then config=du/du2.samefreq-ssb0.conf; fi
        ;;
      hub)
        # the hub runs in the same namespace as the DUs
        args+=('--rfsimulator.[0].serveraddr' 127.0.0.1 '--rfsimulator.[0].serverport' "$hub_port")
        if [[ $node == du2 ]]; then config=du/du2.samefreq-ssb0.conf; fi
        ;;
      *) printf 'Unknown variant: %s\n' "$variant" >&2; exit 1 ;;
    esac
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
