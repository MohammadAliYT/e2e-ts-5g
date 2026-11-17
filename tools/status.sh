#!/usr/bin/env bash
# Read-only overview of the testbed. Usage: sudo ~/e2e-ts-5g/tools/status.sh
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/RAN/interfaces/lab.env"
OAI=$root/upstream/oai; B=$OAI/cmake_targets/ran_build/build; F=$OAI/openair2/E2AP/flexric/build
ok(){ [[ -e "$1" ]] && echo "  [x] $2" || echo "  [ ] $2"; }

echo "== Versions"
echo "  OAI     $(git -C $OAI describe --tags 2>/dev/null) ($(git -C $OAI log -1 --format=%h))"
echo "  FlexRIC $(git -C $OAI/openair2/E2AP/flexric log -1 --format=%h 2>/dev/null)"

echo "== Built / installed"
ok $B/nr-softmodem            "nr-softmodem (CU/DU)"
ok $B/nr-uesoftmodem          "nr-uesoftmodem (UE)"
ok $B/librfsimulator.so       "RFsim"
ok $B/libtelnetsrv.so         "telnet"
ok $F/examples/ric/nearRT-RIC "nearRT-RIC"
ok /usr/local/lib/flexric/libkpm_sm.so "E2 service models"
ok $F/examples/xApp/c/rc_handover/xapp_rc_handover "xApp rc_handover"
lsmod | grep -q gtp5g && echo "  [x] gtp5g kernel module" || echo "  [ ] gtp5g kernel module"
command -v go >/dev/null && echo "  [x] Go $(go version | awk '{print $3}')" || echo "  [ ] Go"
ok $root/upstream/free5gc/bin/amf "free5GC"

echo "== Namespaces"
for ns in $RAN_NS $UE1_NS $UE2_NS $UPF_NS $DN_NS; do
  if ip netns list | grep -qw $ns; then
    echo "  [x] $ns: $(ip netns exec $ns ip -br -4 addr | grep -v '^lo' | awk '{printf "%s %s  ",$1,$3}')"
  else echo "  [ ] $ns"; fi
done

echo "== Running"
n=0
while read -r pid et args; do
  n=1; ns=$(ip netns identify $pid 2>/dev/null); conf=$(grep -oE -- '-O [^ ]+' <<<"$args" | awk '{print $2}')
  printf "  %-7s %-10s %-9s %s\n" "$pid" "${ns:-host}" "$et" "$(basename "${args%% *}") ${conf:+$(basename "$conf")}"
done < <(ps -eo pid=,etime=,args= | grep -E 'nr-softmodem|nr-uesoftmodem|nearRT-RIC|xapp_|free5gc|/bin/(amf|smf|upf|nrf|ausf|udm|udr|pcf|nssf|webui)' | grep -vE 'grep|sudo|tee')
[[ $n == 0 ]] && echo "  (nothing)"

echo "== Listening ports in $RAN_NS"
ip netns exec $RAN_NS ss -Hlntp  2>/dev/null | awk '{print "  tcp  "$4"  "$6}' | grep -E '4043|4044|9090|36421' 
ip netns exec $RAN_NS ss -HlnpS  2>/dev/null | awk '{print "  sctp "$4"  "$6}'
echo "  (4043/4044 RFsim, 38472 F1-C, 36421 E2, 9090 telnet)"
