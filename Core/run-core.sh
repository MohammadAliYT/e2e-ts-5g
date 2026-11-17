#!/usr/bin/env bash
# Baseline free5GC core (start order follows upstream run.sh v4.3.0). Ctrl+C stops all.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
F5=$root/upstream/free5gc; CFG=$root/Core/config
LOG=$root/results/core; mkdir -p "$LOG"
cd "$F5"          # NFs read cert/* relative to the free5gc folder
sudo -v
pids=()
stop() {
  echo "stopping core"
  kill -INT "${pids[@]}" 2>/dev/null
  sudo pkill -INT -f "$F5/bin/upf" 2>/dev/null
  sleep 2         # let the UPF remove upfgtp
  wait; exit 0
}
trap stop INT TERM

# UPF: remove stale gtp5g link from an earlier run, then start in its namespace
sudo ip netns exec "$UPF_NS" ip link del upfgtp 2>/dev/null
sudo ip netns exec "$UPF_NS" "$F5/bin/upf" -c "$CFG/upfcfg.yaml" -l "$LOG/upf.log" &
sleep 2
if ! sudo ip netns exec "$UPF_NS" ss -Hlnu | grep -q ':8805'; then
  echo "UPF failed to start - see $LOG/upf.log"; exit 1
fi
echo "UPF up on PFCP 8805"

mongosh free5gc --quiet --eval 'for (c of ["NfProfile","applicationData.influenceData.subsToNotify","applicationData.subsToNotify","policyData.subsToNotify","exposureData.subsToNotify"]) db.getCollection(c).drop()' >/dev/null

export GIN_MODE=release
for nf in nrf amf smf udr pcf udm nssf ausf chf nef; do
  "$F5/bin/$nf" -c "$CFG/${nf}cfg.yaml" -l "$LOG/$nf.log" &
  pids+=($!); echo "started $nf"; sleep 0.2
done
echo "core up - logs in $LOG - Ctrl+C to stop"
wait