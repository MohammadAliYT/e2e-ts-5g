#!/usr/bin/env bash
# Steps the downlink path loss of one RFsim channel model on a UE (via telnet)
# and snapshots each listed DU's nrMAC_stats.log at the end of each step.
# Usage:   [RUN=<name>] tools/chanmod-sweep.sh <ue1|ue2> <model index> <du list> <hold s> <ploss...>
# Example: RUN=run2 tools/chanmod-sweep.sh ue2 1 du1,du2 30 25 26 27 28 29 30 31 32 33 34 35
# Output: results/chanmod/<RUN>/ (steps.log in Unix time, same clock as "ping -D").
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
ue=$1; idx=$2; hold=$4
IFS=',' read -r -a dus <<<"$3"
shift 4
case "$ue" in
  ue1) ns=$UE1_NS ;;
  ue2) ns=$UE2_NS ;;
  *) printf 'Unknown UE: %s\n' "$ue" >&2; exit 1 ;;
esac
out="$root/results/chanmod/${RUN:-.}"; mkdir -p "$out"
steps="$out/steps.log"
echo "# $(date -Is) ue=$ue model=$idx dus=${dus[*]} hold=${hold}s" >> "$steps"

for p in "$@"; do
  reply=$(printf 'channelmod modify %s ploss %s\r\n' "$idx" "$p" \
          | sudo ip netns exec "$ns" nc -q 1 127.0.0.1 9090 | tr -d '\r')
  if ! grep -q "path loss: ${p}\." <<<"$reply"; then
    printf 'ploss %s not confirmed by telnet, stopping. Reply:\n%s\n' "$p" "$reply" >&2
    exit 1
  fi
  printf '%s ploss %s\n' "$(date +%s.%N)" "$p" | tee -a "$steps"
  sleep "$hold"
  for du in "${dus[@]}"; do
    cp "$root/results/f1-smoke/$du/nrMAC_stats.log" "$out/mac-$du-ploss$p.log"
  done
done
echo "done. Reset with: channelmod modify $idx ploss 10"