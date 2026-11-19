#!/usr/bin/env bash
# UDP traffic at a fixed rate between one UE and the data network (iperf3).
# Usage: tools/traffic.sh <ue1|ue2> <dl|ul> <rate, e.g. 5M> <seconds> [tcp]
#   dl = DN -> UE (iperf3 -R), ul = UE -> DN; tcp = TCP instead of UDP (rate = cap)
# Server: iperf3 in the DN namespace, one port per UE and direction (ue1 dl 5201, ul 5202; ue2 dl 5203, ul 5204),
# serves one client and exits -> DL and UL of the same UE can run at the same time.
# Result: results/traffic/<time>-<ue>-<dir>-<rate>.json and a one-line summary.
# Several flows at once: run several calls with & and then wait.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
[[ $# -ge 4 ]] || { echo "usage: $0 <ue1|ue2> <dl|ul> <rate> <seconds> [tcp]"; exit 1; }
ue=$1 dir=$2 rate=$3 dur=$4 proto=${5:-udp}
case $ue in ue1) ns=$UE1_NS port=5201 ;; ue2) ns=$UE2_NS port=5203 ;; *) echo "ue1|ue2"; exit 1 ;; esac
case $dir in dl) rev=(-R) ;; ul) rev=(); port=$((port + 1)) ;; *) echo "dl|ul"; exit 1 ;; esac
[[ $proto == tcp ]] && mode=(-b "$rate") || mode=(-u -b "$rate")

ip=$(sudo ip netns exec "$ns" ip -4 -o addr show dev "$ue" 2>/dev/null | awk '{print $4}' | cut -d/ -f1)
[[ -n $ip ]] || { echo "$ue has no IP"; exit 1; }
out="$root/results/traffic"; mkdir -p "$out"
f="$out/$(date +%Y%m%d-%H%M%S)-$ue-$dir-$rate-$proto.json"

sudo ip netns exec "$DN_NS" pkill -f "iperf3 -s -p $port" 2>/dev/null
sudo ip netns exec "$DN_NS" iperf3 -s -p "$port" -1 -D
sleep 0.5
sudo ip netns exec "$ns" iperf3 -c "$DN_IP" -p "$port" -B "$ip" "${mode[@]}" "${rev[@]}" \
  -t "$dur" -l 1200 -J >"$f" 2>&1

python3 -I - "$f" "$ue" "$dir" "$rate" "$proto" <<'EOF'
import json, sys
f, ue, d, rate, proto = sys.argv[1:]
try:
    j = json.load(open(f))
except Exception as e:
    sys.exit(f"{ue} {d}: no iperf3 result ({e}) - see {f}")
if "error" in j:
    sys.exit(f"{ue} {d}: iperf3 error: {j['error']}")
e = j["end"]
s = e.get("sum") or e.get("sum_received") or {}
r = e.get("sum_received", s)
line = f"{ue} {d} {proto} target {rate}: received {r.get('bits_per_second', 0)/1e6:.2f} Mbit/s"
if proto == "udp":
    line += f", loss {s.get('lost_percent', 0):.2f} %, jitter {s.get('jitter_ms', 0):.2f} ms"
print(line + f"  ({f})")
EOF