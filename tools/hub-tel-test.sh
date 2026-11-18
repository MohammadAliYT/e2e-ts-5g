#!/usr/bin/env bash
# Hub telemetry test (T1): does the DU telemetry follow the hub channel?
# Lab must be up in hub mode with UE1 on DU1.
#   1. sets the receiver noise floor on all 4 nodes (default -75 dBFS, env NOISE)
#   2. path-loss sweep UE1-DU1: 0 10 15 20 25 30 35 dB, with ping load (100 pkt/s, 1000 bytes),
#      DU2 out of reach for UE1 (UE1-DU2 40 dB: no interference, UE1 cannot re-establish on DU2)
#      stops at the first step with ping loss > 20 %, BLER > 0.3 or UE1 not in-sync on DU1,
#      and moves UE1 back (0 dB)
#   3. interference steps at UE1-DU1 0 dB: UE1-DU2 10 dB (normal start), 6 dB
#   restore: UE1-DU1 0 dB, UE1-DU2 10 dB; the noise floor stays set
# Values per step: UE1 block of DU1's nrMAC_stats.log (the scheduler data the E2 MAC service
# model reports): RSRP, PH, PUCCH SNR, DL MCS/BLER, PUSCH SNR, UL MCS/BLER, goodput, ping loss.
# Usage: [NOISE=-75] tools/hub-tel-test.sh
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
cd "$root"
export LC_NUMERIC=C
noise=${NOISE:--75}
out="$root/results/hub-tel"; mkdir -p "$out"
log="$out/run-$(date +%Y%m%d-%H%M%S).txt"
stats="results/f1-smoke/du1/nrMAC_stats.log"

ctrl() { echo "$*" | sudo ip netns exec "$RAN_NS" nc -q 1 127.0.0.1 4099; }
val()  { local v; v=$(grep -o "$1" <<<"$2" | head -1 | awk "{print \$$3}"); echo "${v:--}"; }
load() { # 8 s ping load from UE1, prints loss in %
  local p
  p=$(sudo ip netns exec "$UE1_NS" ping -q -i 0.01 -s 1000 -W 1 -w 8 -I ue1 "$DN_IP" 2>/dev/null \
      | grep -o '[0-9.]*% packet loss' | cut -d% -f1)
  echo "${p:-100}"; }

snap() { # <step name>; returns 1 if the link is degraded
  local b h dl ul gp p
  sleep 2; p=$(load)
  b=$(grep -A4 'CU-UE-ID 1 in-sync' "$stats" 2>/dev/null)
  [[ -z $b ]] && { sleep 1; b=$(grep -A4 'CU-UE-ID 1 in-sync' "$stats" 2>/dev/null); }  # file being rewritten
  [[ -z $b ]] && b="(UE1 not in-sync on DU1)"
  echo "## $1" >>"$log"; echo "$b" >>"$log"
  h=$(head -1 <<<"$b"); dl=$(grep dlsch_rounds <<<"$b"); ul=$(grep ulsch_rounds <<<"$b")
  gp=$(grep goodput <<<"$b" | awk '{for(i=1;i<=NF;i++){if($i=="DL")d=$(i+1);if($i=="UL")u=$(i+1)}} END{print d"/"u}')
  local dbl ubl
  dbl=$(val 'BLER [0-9.]*' "$dl" 2); ubl=$(val 'BLER [0-9.]*' "$ul" 2)
  printf '%-16s %-5s %-3s %-6s %-4s %-7s %-6s %-4s %-7s %-11s %s\n' "$1" \
    "$(val 'average RSRP [-0-9]*' "$h" 3)" "$(val 'PH [-0-9]*' "$h" 2)" \
    "$(val 'SNR [-0-9.]*' "$dl" 2)" "$(val 'MCS ([0-9]) [0-9]*' "$dl" 3)" "$dbl" \
    "$(val 'SNR [-0-9.]*' "$ul" 2)" "$(val 'MCS ([0-9]) [0-9]*' "$ul" 3)" "$ubl" \
    "${gp:--}" "$p%" | tee -a "$log"
  [[ $dbl != - ]] || return 1                                        # UE1 left DU1
  awk -v p="$p" -v d="$dbl" -v u="${ubl/-/0}" 'BEGIN { exit !(p <= 20 && d <= 0.3 && u <= 0.3) }'
}

if ! grep -q 'CU-UE-ID 1 in-sync' "$stats" 2>/dev/null; then
  echo "UE1 (CU UE ID 1) is not in-sync on DU1 - (re)start the lab first"; exit 1
fi
echo "# $(date -Is) hub telemetry test (UE1 on DU1), noise floor $noise dBFS" | tee -a "$log"
for n in du1 du2 ue1 ue2; do ctrl noise "$n" "$noise" >/dev/null; done
ctrl loss ue1 du1 0 >/dev/null; ctrl loss ue1 du2 40 >/dev/null
printf '%-16s %-5s %-3s %-6s %-4s %-7s %-6s %-4s %-7s %-11s %s\n' \
  step RSRP PH PUCCH DLMC DL-BLER PUSCH ULMC UL-BLER "Mbps DL/UL" ping | tee -a "$log"

for l in 0 10 15 20 25 30 35; do
  ctrl loss ue1 du1 "$l" >/dev/null
  snap "loss $l dB" || { echo "  degraded at $l dB - UE1 back to 0 dB" | tee -a "$log"; break; }
done
ctrl loss ue1 du1 0 >/dev/null; snap "back at 0 dB"
ctrl loss ue1 du2 10 >/dev/null; snap "interf. 10 dB"
ctrl loss ue1 du2 6 >/dev/null;  snap "interf. 6 dB"
ctrl loss ue1 du2 10 >/dev/null
ctrl show | tee -a "$log"
echo "log (with the full MAC blocks): $log"
