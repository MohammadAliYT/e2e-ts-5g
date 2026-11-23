#!/usr/bin/env bash
# Cell capacity test: UDP rate ramp for UE1 on its serving DU.
# Case A: the other cell is idle. Case B: UE2 loads the other cell.
# The controller runs during the test and logs PRB use, MCS, BLER and the DL queue every second.
# The controller has no decide step, so it does not start handovers.
# Settings (environment variables, defaults below):
#   CASES                          cases to run: "A B", "A" or "B"
#   DL_RATES_A UL_RATES_A          rate ramps for case A (iperf3 -b values)
#   DL_RATES_B UL_RATES_B          rate ramps for case B
#   STEP_S                         duration of one step in s
#   PAUSE_S                        pause after a step in s (the queue empties)
#   BG_DL BG_UL                    UE2 load in case B (DL during the DL ramp, UL during the UL ramp)
# Usage (runs in the background, survives an SSH drop):
#   sudo -v && (sudo setsid nohup tools/cap-test.sh > /tmp/cap-run.log 2>&1 < /dev/null &)
#   With settings: ... sudo setsid nohup env CASES=B tools/cap-test.sh ...
# Result: results/cap/run-<time>/ (link: results/cap/latest):
#   run.log, settings.txt, ctrl.csv, ctrl.log, ue1-<dl|ul>-<case>.txt
# Summary: python3 -I tools/cap-summary.py
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
ctrl="$root/upstream/oai/openair2/E2AP/flexric/build/examples/xApp/c/e2e_ctrl/e2e_ctrl"
CASES=${CASES:-A B}
DL_RATES_A=${DL_RATES_A:-10M 20M 30M 40M 50M 60M 80M}
UL_RATES_A=${UL_RATES_A:-2M 4M 5M 6M 7M 8M}
DL_RATES_B=${DL_RATES_B:-6M 8M 10M 12M 14M 16M 20M}
UL_RATES_B=${UL_RATES_B:-2M 3M 4M 5M 6M 7M}
STEP_S=${STEP_S:-15}
PAUSE_S=${PAUSE_S:-5}
BG_DL=${BG_DL:-5M}
BG_UL=${BG_UL:-1M}
user=${SUDO_USER:-root}
ctrl_pid=""

[[ $EUID -eq 0 ]] || { echo "start the script with sudo (see usage)"; exit 1; }
[[ -x $ctrl ]] || { echo "controller not built: $ctrl"; exit 1; }
pgrep -x e2e_ctrl >/dev/null && { echo "e2e_ctrl runs already - stop it first (one xApp at a time)"; exit 1; }

run="$root/results/cap/run-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run" && ln -sfn "$run" "$root/results/cap/latest"
exec > >(tee "$run/run.log") 2>&1
printf '%s\n' "CASES=$CASES" "DL_RATES_A=$DL_RATES_A" "UL_RATES_A=$UL_RATES_A" \
  "DL_RATES_B=$DL_RATES_B" "UL_RATES_B=$UL_RATES_B" "STEP_S=$STEP_S" "PAUSE_S=$PAUSE_S" \
  "BG_DL=$BG_DL" "BG_UL=$BG_UL" >"$run/settings.txt"

# Stop the UE2 load and the controller. Runs at the end and if the script stops early.
cleanup() {
  pkill -INT -f "iperf3 -c [^ ]+ -p 520[34] " 2>/dev/null
  if [[ -n $ctrl_pid ]]; then
    kill -INT "$ctrl_pid" 2>/dev/null
    wait "$ctrl_pid" 2>/dev/null
    ctrl_pid=""
  fi
}
trap cleanup EXIT

# Send a rate ramp from UE1. Arguments: <dl|ul> <rates> <result name>.
ramp() {
  for r in $2; do
    "$root/tools/traffic.sh" ue1 "$1" "$r" "$STEP_S"
    sleep "$PAUSE_S"
  done 2>&1 | tee "$run/$3.txt"
}

# Start the UE2 load in the other cell. Arguments: <dl|ul> <rate>.
# The load runs until bg_stop. iperf3 writes its result on SIGINT.
bg_start() {
  "$root/tools/traffic.sh" ue2 "$1" "$2" 3600 >"$run/bg-ue2-$1.txt" 2>&1 &
  bg_pid=$!
  sleep 5
}

# Stop the UE2 load. Argument: <dl|ul>.
bg_stop() {
  kill -0 "$bg_pid" 2>/dev/null || echo "WARNING: UE2 load ended before the ramp ended - case B is not valid"
  local port=5203
  [[ $1 == ul ]] && port=5204
  pkill -INT -f "iperf3 -c [^ ]+ -p $port " 2>/dev/null
  wait "$bg_pid" 2>/dev/null
  sleep "$PAUSE_S"
}

echo "== start controller ($run)"
E2E_CSV="$run/ctrl.csv" XAPP_DURATION=7200 ip netns exec "$RAN_NS" "$ctrl" >"$run/ctrl.log" 2>&1 &
ctrl_pid=$!
sleep 5
kill -0 "$ctrl_pid" 2>/dev/null || { echo "controller stopped - see $run/ctrl.log"; exit 1; }

for c in $CASES; do
  case $c in
    A)
      echo "== case A: other cell idle"
      ramp dl "$DL_RATES_A" ue1-dl-A
      ramp ul "$UL_RATES_A" ue1-ul-A
      ;;
    B)
      echo "== case B: other cell loaded by UE2 (DL $BG_DL, then UL $BG_UL)"
      bg_start dl "$BG_DL"; ramp dl "$DL_RATES_B" ue1-dl-B; bg_stop dl
      bg_start ul "$BG_UL"; ramp ul "$UL_RATES_B" ue1-ul-B; bg_stop ul
      ;;
    *)
      echo "unknown case: $c"
      ;;
  esac
done

echo "== stop controller"
cleanup
chown -R "$user": "$run" "$root/results/traffic"
chown -h "$user": "$root/results/cap/latest"
echo "== done. Summary: python3 -I tools/cap-summary.py"