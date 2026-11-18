#!/usr/bin/env bash
# Stop in order: xApps -> UE1, UE2 -> DU2 -> DU1 -> CU -> RIC  (core keeps running)
# tools/lab-down.sh all  -> also webconsole + core
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
F5=$root/upstream/free5gc
sudo -v || exit 1

stop() { # <label> <pattern>
  pgrep -f "$2" >/dev/null || { echo "  -     $1 (not running)"; return; }
  sudo pkill -TERM -f "$2"
  for i in {1..10}; do pgrep -f "$2" >/dev/null || { echo "  stop  $1"; return; }; sleep 1; done
  sudo pkill -KILL -f "$2"; sleep 1; echo "  kill  $1 (ignored TERM)"; }

stop xApps '[x]app_'
# stop UE1 'nr-uesoftmodem.*ue1\.conf'
# stop UE2 'nr-uesoftmodem.*ue2\.conf'
# stop DU2 'nr-softmodem.*du2\.conf'
# stop DU1 'nr-softmodem.*du1\.conf'
# stop CU  'nr-softmodem.*cu\.conf'
stop UE1 'nr-uesoftmodem.*UE/ue1\.'
stop UE2 'nr-uesoftmodem.*UE/ue2\.'
stop DU2 'nr-softmodem.*du2\.'
stop DU1 'nr-softmodem.*du1\.'
stop HUB 'rfsim-[h]ub'
stop CU  'nr-softmodem.*cu\.'
stop RIC '[n]earRT-RIC'
if [[ ${1:-} == all ]]; then
  stop webconsole 'bin/[w]ebconsole'
  stop core "$F5/[b]in/"
fi