#!/usr/bin/env bash
# Start one OAI UE in its namespace.
# Usage: run-ue.sh <ue1|ue2> [chanmod|gate|gate-chanmod] [extra nr-uesoftmodem options...]
#   chanmod:      UE/<ue>.chanmod.conf, RFsim channel model on both RUs (downlink from DU1 and DU2),
#                 telnet server on port 9090 inside the UE namespace.
#   gate:         single-radio test (UE1 only). UE/<ue>.gate.conf, one radio, the UE is the
#                 RFsim server on port 4043; DU1 and DU2 connect to it as clients.
#   gate-chanmod: like gate, plus one channel model per connected DU (UE/<ue>.gate-chanmod.conf;
#                 rfsimu_channel_ue0 = first DU, ue1 = second DU) and telnet on port 9090.
#   Extra options are passed on unchanged, e.g. --log_config.nr_rrc_log_level debug
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
ue="${1:?Pass ue1 or ue2}"
variant="${2:-}"
if [[ $# -ge 2 ]]; then shift 2; else shift $#; fi   # what is left: extra options
case "$ue" in
  ue1) ns=$UE1_NS ;;
  ue2) ns=$UE2_NS ;;
  *) printf 'Unknown UE: %s\n' "$ue" >&2; exit 1 ;;
esac

conf="$root/UE/$ue.conf"
extra=()
case "$variant" in
  "") ;;
  chanmod)
    conf="$root/UE/$ue.chanmod.conf"
    # quoted: [0] and [1] would otherwise be shell glob patterns
    extra=('--rfsimulator.[0].options' chanmod '--rfsimulator.[1].options' chanmod --telnetsrv)
    ;;
  gate|gate-chanmod)
    # single radio: UE is the RFsim server on port 4043, tuned to DU1's cell (3450.72 MHz)
    conf="$root/UE/$ue.$variant.conf"
    extra=(-C 3450720000 -r 106 --numerology 1 --band 78 --ssb 516
           '--rfsimulator.[0].serveraddr' server '--rfsimulator.[0].serverport' 4043)
    if [[ $variant == gate-chanmod ]]; then extra+=('--rfsimulator.[0].options' chanmod --telnetsrv); fi
    ;;
  *) printf 'Unknown variant: %s\n' "$variant" >&2; exit 1 ;;
esac
extra+=("$@")
[[ -f "$conf" ]] || { printf 'Missing config: %s\n' "$conf" >&2; exit 1; }

work="$root/results/f1-smoke/$ue"
mkdir -p "$work"; cd "$work"
sudo ip netns exec "$ns" \
  "$root/upstream/oai/cmake_targets/ran_build/build/nr-uesoftmodem" \
  -O "$conf" --rfsim "${extra[@]}" 2>&1 | tee "$ue.log"