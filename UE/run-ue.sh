#!/usr/bin/env bash
# Start one OAI UE (user equipment) in its network namespace, in hub mode.
#
# Usage: UE/run-ue.sh <ue1|ue2> [extra nr-uesoftmodem options...]
#   The UE has one radio and is an RFsim (RF simulator) client of tools/rfsim-hub:
#   UE1 -> port 4051, UE2 -> port 4052, reached via the RAN-side address of the UE link.
#   In hub mode both DUs (distributed units) send on 3450.72 MHz.
#   SIM: UE/<ue>.hub.conf. Radio settings: below.
#   Extra options are passed on unchanged, e.g. --log_config.nr_rrc_log_level debug
#   The old modes (normal, chanmod, gate, gate-chanmod) are in the git history.

set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"

ue="${1:?Pass ue1 or ue2}"
shift   # what is left: extra options

case "$ue" in
  ue1) ns=$UE1_NS; ran_ip=$UE1_RAN_IP; hub_port=4051 ;;
  ue2) ns=$UE2_NS; ran_ip=$UE2_RAN_IP; hub_port=4052 ;;
  *) printf 'Unknown UE: %s\n' "$ue" >&2; exit 1 ;;
esac

conf="$root/UE/$ue.hub.conf"
[[ -f "$conf" ]] || { printf 'Missing config: %s\n' "$conf" >&2; exit 1; }

# Radio: band n78, 3450.72 MHz, 106 PRBs, numerology 1 (30 kHz subcarrier spacing), SSB offset 516
radio=(-C 3450720000 -r 106 --numerology 1 --band 78 --ssb 516)
# RFsim client of the hub (quoted: [0] would otherwise be a shell glob pattern)
rfsim=('--rfsimulator.[0].serveraddr' "$ran_ip" '--rfsimulator.[0].serverport' "$hub_port")

work="$root/results/f1-smoke/$ue"
mkdir -p "$work"; cd "$work"
# E2E_NEIGH_DIAG=1: neighbour-measurement diag lines (oai-ue-neigh-known-pci.patch); sudo drops it, so pass it on
sudo ip netns exec "$ns" env E2E_NEIGH_DIAG="${E2E_NEIGH_DIAG:-0}" \
  "$root/upstream/oai/cmake_targets/ran_build/build/nr-uesoftmodem" \
  -O "$conf" --rfsim "${radio[@]}" "${rfsim[@]}" "$@" 2>&1 | tee "$ue.log"