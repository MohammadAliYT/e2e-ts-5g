#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
ue="${1:?Pass ue1 or ue2}"
case "$ue" in
  ue1) ns=$UE1_NS ;;
  ue2) ns=$UE2_NS ;;
  *) printf 'Unknown UE: %s\n' "$ue" >&2; exit 1 ;;
esac
work="$root/results/f1-smoke/$ue"
mkdir -p "$work"; cd "$work"
sudo ip netns exec "$ns" \
  "$root/upstream/oai/cmake_targets/ran_build/build/nr-uesoftmodem" \
  -O "$root/UE/$ue.conf" --rfsim 2>&1 | tee "$ue.log"
