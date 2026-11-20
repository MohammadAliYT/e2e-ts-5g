# RAN TS milestone – checklist

Updated: 2026-11-20 17:45. Single source of truth for all Claude sessions: read this first, update it after each step. Spell out every abbreviation in brackets.

## Done
- [x] RFsim relay hub (3c): 1 radio per UE (user equipment), 2 DUs (distributed units) × 2 UEs
- [x] H1–H3 hub tests; handover test 4/4 PASS (committed)
- [x] Telemetry follows hub channel (DU MAC (Medium Access Control) stats)
- [x] Noise floor −75 dBFS calibrated, hub default in `lab-up.sh` (committed)
- [x] Monitor patched: no PDCP/GTP subscription on the CU (central unit) (`Controller/patches/flexric-moni-no-cu-gtp.patch`)
- [x] **E2 MAC SM (service model) follows the hub** (19 Nov 13:22): UE1 `dl_mcs1` 28 → 13 → 0, `dl_bler` up to 0.89; `pusch_snr` 32 → 8 dB
- [x] DU identity in E2: `nb_id` DU1 = 3584, DU2 = 3585 (`cu_du_id` identical – don't use)
- [x] Traffic generator `tools/traffic.sh` (iperf3, fixed rate, per UE and direction, ports 5201–5204)
- [x] **E2 data sources checked against iperf3** (UE1 5 DL / 2 UL, UE2 3 DL / 1 UL Mbit/s, 0 % loss):

| Source | Value | Result | Use in controller |
|---|---|---|---|
| KPM (Key Performance Measurement) | `DRB.UEThpDl` | 5149 / 3091 kbps median ✅, spikes up to 37,320 | DL throughput, median filter |
| KPM | `DRB.UEThpUl` | ≈ 0.6 kbps ❌ – cause in source: OAI `ran_func_kpm_subs.c:270` reads `rb_id = 1`, UL data is on DRB 2 | no |
| KPM | `RRU.PrbTotDl` / `RRU.PrbTotUl` (PRB = physical resource block use) | DL 20 / 12 %, UL 65 / 54 % ✅ | cell load |
| MAC SM | `dl_aggr_tbs` (Δ) | 5.24 / 3.14 Mbit/s ✅ (+4 % headers) | DL throughput |
| MAC SM | `ul_aggr_tbs` (Δ) | 2.7–2.9 / 1.6–1.7 Mbit/s ❌ (+35–60 %, padding) | no |
| MAC SM | `dl/ul_aggr_bytes_sdus`, `ul_sched_rb` | 0 ❌ | no |
| MAC SM | MCS, BLER, PUSCH/PUCCH SNR | follow the hub ✅ (absolute PUSCH SNR value unexplained, see open items) | radio quality |
| **RLC (Radio Link Control) SM** | `txpdu_bytes` (Δ, DRB 1) | 5.15 / 3.09 Mbit/s ✅ | **DL throughput** |
| **RLC SM** | `rxpdu_bytes` (Δ, DRB 2) | 2.07 / 1.04 Mbit/s ✅ | **UL throughput** |

- [x] **RNTI (Radio Network Temporary Identifier) ↔ CU UE ID in KPM**: OAI patch `Controller/patches/oai-kpm-du-rnti.patch`, nr-softmodem rebuilt; DU KPM reports `gnb_cu_ue_f1ap` = CU UE ID and `ran_ue_id` = RNTI (fac8 / ef02, 19 Nov 15:25)
- [x] **Neighbour measurement in the UE (user equipment) PASS** (20 Nov 17:30; details `handover-2026-11-20.md` §9):
  4 UE patches in order: `oai-ue-cell-rsrp.patch`, `oai-ue-neigh-detect.patch`, `oai-ue-neigh-known-pci.patch`,
  `oai-ue-rrc-rsrp-report.patch`. Test `tools/neigh-meas-test.sh` (needs `E2E_NEIGH_DIAG=1 tools/lab-up.sh`), both UEs:
  0/3/6/10 dB → 82-bit reports only, LOST 0, timing 4560 (serving SSB (synchronization signal block) timing),
  CFAR (constant false-alarm rate) ratio ~20–22; RSRP (reference signal received power) gap serving − neighbour
  0/3/6/10 dB at UE PHY (physical layer) and at the CU (central unit) `nrRRC_stats.log` (CU −41 / −41…−51 dBm);
  60 dB → LOST 1, then serving only (LOST after 2–5 s); 25 dB detected (earlier run). Handover regression
  `HO_TOOL=tools/ho-ctrl.sh tools/hub-ho-test.sh --no-start` 4/4 PASS (HO 1.8–2.0 s, 0 % loss, no re-establishment)
- [x] Controller language decided: **C**. FlexRIC Python bindings (SWIG) are not built here (`XAPP_PYTHON_SWIG` absent in CMakeCache, `build/examples/xApp/python3/` has no SDK); RC handover + F1 decoding exist only in C. Python only for result analysis.

## Now
- [ ] Controller v0, stage 1 "Observe" (see `handover-2026-11-19.md` §5–7)
  - [x] Source read: monitor, rc_handover, `src/xApp`, `src/sm` (MAC/RLC/KPM), OAI `ran_func_{mac,rlc,kpm,kpm_subs}.c` (19 Nov 16:45)
  - [x] Written: `Controller/xapp/e2e_ctrl/` (`e2e_ctrl.c`, `CMakeLists.txt`, `README.md`); symlinked into `…/flexric/examples/xApp/c/e2e_ctrl`; hook `Controller/patches/flexric-add-e2e-ctrl.patch`
  - [x] Built: `cmake --build …/flexric/build --target e2e_ctrl` OK, no warnings (19 Nov 16:48)
  - [x] **Lab test PASS** (19 Nov 17:02, hub defaults, `results/ctrl/obs-20261119-170209.{csv,log}`, iperf3 JSON `results/traffic/20261119-170219-*`): 3 E2 nodes, CU skipped, DU1 = 3584 / cell 12345678, DU2 = 3585 / cell 11111111; 6/6 subscriptions OK; RNTI fac8 = CU UE ID 1 (DU1), ef02 = CU UE ID 2 (DU2); 100 MAC + 100 RLC + 1 KPM samples per s, 0 RLC resets, clean exit
    | UE | iperf3 (0 % loss) | CSV `dl_mbps` / `ul_mbps` (median) | KPM `DRB.UEThpDl` median | MCS DL/UL, BLER, PUSCH SNR | UE PRB DL / UL |
    |---|---|---|---|---|---|
    | UE1 (DU1) | 5.00 DL / 2.00 UL | 5.149 / 2.074 | 5150 kbps | 16 / 16, 0.08, 19.5 dB | 19 / 67 % |
    | UE2 (DU2) | 3.00 DL / 1.00 UL | 3.091 / 1.038 | 3093 kbps | 15 / 16, 0.08–0.10, 16.9 dB | 12 / 52 % |
    - `e2e-db.py` on the same run's DB (`/tmp/xapp_db_1795104129596756`) shows the same MAC values (MCS, BLER, SNR); MAC `dl_aggr_tbs` 5.24 / 3.14 (+4 %), `ul_aggr_tbs` 2.8 / 1.65 (padding), as before
    - KPM `DRB.UEThpDl` max this run 6002 kbps (no 37,320 spikes, single KPM subscriber). Possible link to the shared static arrays: unverified
    - Idle: MCS 0, PUSCH SNR 32.5 dB, UE UL PRB 4–8 % (scheduling of an idle UE)
    - First CSV row of a UE has an empty `cu_ue_id` until the first KPM report (≤ 1 s)
  - Design facts: per-DU callbacks (FlexRIC `sm_cb` has no user pointer); MAC/RLC `10_ms` (only 1/2/5/10 ms accepted); KPM style 4 only, 1000 ms; DU numbering by ascending `nb_id`
  - Don't run `xapp_kpm_moni` together with e2e_ctrl: OAI DU keeps KPM previous values in static arrays shared by all subscribers (`ran_func_kpm_subs.c:198-199`)

## Next
- [ ] Controller v0 stages 2–5 (state, act on command, decide, experiment hooks)
- [ ] Cell capacity per DU (max DL / UL) as reference for the load scenario
- [ ] Scenario 1: load / capacity (vs B0)
- [ ] Scenario 2: QoS (quality of service) / service
- [ ] Scenario 3: mobility (hub trajectories, 0–25 dB)
- [ ] Baselines B0 / B1 / B2 / E2E, all through the hub
- [ ] Handover interruption from logs (RRCReconfiguration → Complete)

## Open items
| Item | Priority | Why |
|---|---|---|
| UE L3 filter coefficient `1 / pow(2, k / 4)` uses integer division (`rrc_UE.c:1507`): fc6 → 1/2 instead of 1/2^1.5 | Low | changes only how fast the reported RSRP follows; matters for B1 (OAI A3) timing |
| Neighbour LOST took up to ~5 s when a keep check passed by chance (absent ratio up to 1.64 > old keep 1.5) | Fixed | keep raised to 2.0; retest 2026-11-20 18:20: LOST without extra report, both UEs |
| E2 `pusch_snr` ≈ 31–33 dB vs DU log "SNR" ≈ 22 dB at 0 dB path loss: **unexplained**. "DU log = power-control average" was an unverified guess – check OAI source (`ran_func_mac.c` pusch_snr vs `dump_mac_stats` SNR) | Medium | thresholds in the decision rules use absolute SNR |
| Cell coupling under load (MCS ~13–16 with both cells loaded): **no control run yet** (same flows, one cell loaded vs both) | Medium | not thesis-ready until the control run exists |
| Two FlexRIC xApps started at the same moment → `init_db_sqlite3` WAL assert | Low | Start xApps a few seconds apart; controller is one xApp |
| KPM `DRB.UEThpUl` ≈ 0; `DRB.UEThpDl` spikes | Medium | Standard O-RAN metric for the thesis; RLC SM as source |
| CU crashes on GTP SM subscription (`read_gtp_sm`, `ran_func_gtp.c:56`, assert 1 QoS flow; 2 DRBs) | Medium | Never subscribe GTP SM (148) on the CU |
| AMF rejects re-registration after complete link loss | Medium | Hits B0 if a UE fully loses the link; tests stay < 25 dB |
| No CQI (Channel Quality Indicator) | Low | MCS + BLER + E2 PUSCH SNR instead |
| `ho.sh` 6 s run time | Low | persistent controller avoids it |

## Observations for the thesis
- Per PDU session there are 2 DRBs (data radio bearers): **DL data on DRB 1, UL data on DRB 2** (RLC counters, 19 Nov 15:08). Relevant for the QoS scenario; cause (free5GC QoS flow rules) still to confirm.
- (needs control run) With all 4 flows (both cells loaded), MCS ≈ 13–16 and BLER ≈ 8–10 % on both UEs: suggests cross-cell interference under load.
- Load-dependent interference: UE1 loaded and within reach of DU2 (10/6 dB) → UE2 `pusch_snr` 32 → 17 dB, `dl_bler` up to 0.25.
- Ping RTT (round-trip time) through the hub under load: avg 38 ms, max 113 ms, loss < 0.1 %.
- UL needs far more PRBs than DL for the same rate (2 Mbit/s UL ≈ 65 % vs 5 Mbit/s DL ≈ 20 %) → UL is the capacity bottleneck in this TDD (time division duplex) configuration.
