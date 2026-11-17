#!/usr/bin/env bash
# Usage: sudo ./netns.sh up|down|status
set -euo pipefail
source "$(dirname "$0")/lab.env"

mkns() { ip netns list | grep -qw "$1" || ip netns add "$1"; ip netns exec "$1" ip link set lo up; }

# veth between two namespaces: $1 nsA $2 ifA $3 ipA/len  $4 nsB $5 ifB $6 ipB/len
link_ns() {
  if ! ip netns exec "$1" ip link show "$2" &>/dev/null; then
    ip link add "$2" type veth peer name "$5"
    ip link set "$2" netns "$1"; ip link set "$5" netns "$4"
    ip netns exec "$1" ip addr add "$3" dev "$2"
    ip netns exec "$4" ip addr add "$6" dev "$5"
  fi
  ip netns exec "$1" ip link set "$2" up; ip netns exec "$4" ip link set "$5" up
}

# veth from a namespace to the core bridge: $1 ns  $2 suffix  $3 ip/len
link_br() {
  if ! ip link show "v-br-$2" &>/dev/null; then
    ip link add "v-br-$2" type veth peer name "v-$2-core"
    ip link set "v-$2-core" netns "$1"
    ip link set "v-br-$2" master "$CORE_BR"
    ip netns exec "$1" ip addr add "$3" dev "v-$2-core"
  fi
  ip link set "v-br-$2" up; ip netns exec "$1" ip link set "v-$2-core" up
}

case "${1:-}" in
  up)
    mkns "$RAN_NS"; mkns "$UE1_NS"; mkns "$UE2_NS"; mkns "$UPF_NS"; mkns "$DN_NS"
    # UE <-> RAN (RFsim "air")
    link_ns "$RAN_NS" v-ran-ue1 "$UE1_RAN_IP/24" "$UE1_NS" v-ue1 "$UE1_IP/24"
    link_ns "$RAN_NS" v-ran-ue2 "$UE2_RAN_IP/24" "$UE2_NS" v-ue2 "$UE2_IP/24"
    # core bridge on host (AMF N2, SMF N4)
    ip link show "$CORE_BR" &>/dev/null || ip link add "$CORE_BR" type bridge
    ip addr show dev "$CORE_BR" | grep -q "$CORE_BR_IP/" || ip addr add "$CORE_BR_IP/24" dev "$CORE_BR"
    ip link set "$CORE_BR" up
    link_br "$RAN_NS" ran "$RAN_CORE_IP/24"      # CU N2/N3
    link_br "$UPF_NS" upf "$UPF_CORE_IP/24"      # UPF N3/N4
    # UPF <-> DN (N6)
    link_ns "$UPF_NS" v-upf-dn "$UPF_N6_IP/24" "$DN_NS" v-dn "$DN_IP/24"
    ip netns exec "$UPF_NS" sysctl -qw net.ipv4.ip_forward=1
    ip netns exec "$DN_NS"  ip route replace "$UE_POOL" via "$UPF_N6_IP"
    # Docker/br_netfilter: let traffic pass within br-core
    if [[ "$(sysctl -n net.bridge.bridge-nf-call-iptables 2>/dev/null || echo 0)" == 1 ]]; then
      iptables -C FORWARD -i "$CORE_BR" -o "$CORE_BR" -j ACCEPT 2>/dev/null \
        || iptables -I FORWARD -i "$CORE_BR" -o "$CORE_BR" -j ACCEPT
    fi
    ;;
  down)   # RAN ns is kept; deleting a ns removes its veths
    for ns in "$UE1_NS" "$UE2_NS" "$UPF_NS" "$DN_NS"; do ip netns del "$ns" 2>/dev/null || true; done
    ip link del v-br-ran 2>/dev/null || true
    ip link del "$CORE_BR" 2>/dev/null || true
    ;;
  status)
    ip netns list
    echo "== host $CORE_BR"; ip -br addr show dev "$CORE_BR" 2>/dev/null || echo "  missing"
    for ns in "$RAN_NS" "$UE1_NS" "$UE2_NS" "$UPF_NS" "$DN_NS"; do
      echo "== $ns"; ip netns exec "$ns" ip -br addr | grep -v '^lo'
    done
    echo "== $DN_NS routes"; ip netns exec "$DN_NS" ip route
    ;;
  *) echo "Usage: $0 up|down|status" >&2; exit 1 ;;
esac
