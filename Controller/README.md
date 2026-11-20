# Controller

The E2E (end-to-end) traffic steering controller of the thesis. It runs as an **xApp**
on FlexRIC's near-RT RIC (RAN Intelligent Controller) and will later also talk to the
free5GC core.

Current state: **phase 3 "Act on command"** (on top of phase 2 "State"): it reads MAC (Medium Access Control),
RLC (Radio Link Control) and KPM (Key Performance Measurement) reports from both DUs
(Distributed Units) and KPM from the CU (Central Unit). Every second it writes one CSV
row per DU entry (DU, RNTI). It also keeps one entry per UE (User Equipment), keyed by
the CU UE ID: serving DU, current RNTI (Radio Network Temporary Identifier), handovers,
active/idle and medians. It writes this view to a second CSV (state CSV).
On a manual command (FIFO, see "Commands") it sends an RC (RAN Control) handover to the
CU with an explicit target cell, and confirms it from the UE view. It makes no
decisions of its own yet (phase 4).

**Never subscribe GTP (148) or PDCP (144) on the CU**: GTP crashed the CU on 19 Nov
(`read_gtp_sm` asserts one QoS flow per PDU session; the lab has 2 DRBs).
`e2/subscribe_cu.c` stops the controller if anyone tries.

## Folder layout

```
Controller/
  README.md                this file
  patches/                 changes to FlexRIC / OAI, kept as git diffs
  xapp/e2e_ctrl/           the controller xApp (C)
    main.c                 start -> find DUs -> subscribe -> write CSV -> stop
    config.h               every fixed setting (intervals, slice, DRB numbers, table sizes)
    CMakeLists.txt         build as part of FlexRIC
    e2/                    talking to the RAN over E2
      nodes.c/.h           list E2 nodes, keep the DUs, sort by nb_id (DU1 = 3584, DU2 = 3585)
      f1_setup.c/.h        F1 Setup Request (from the E2 node config) -> NR Cell ID
      ran_func.c/.h        find a RAN function (service model) in a node's list
      kpm_sub.c/.h         KPM REPORT style 4 subscription (per UE, slice filter), DU and CU
      subscribe_du.c/.h    MAC + RLC + KPM per DU, callbacks -> collect/
      subscribe_cu.c/.h    KPM on the CU, with the GTP/PDCP guard
      rc_handover.c/.h     RC CONTROL style 3 Handover Control to the CU (explicit target NR CGI)
    collect/               one file per report type: E2 report -> state
      mac.c/.h             MCS, BLER, PUSCH/PUCCH SNR
      rlc.c/.h             byte counters -> DL bytes (DRB 1), UL bytes (DRB 2)
      kpm_values.c/.h      walks the (name, value) pairs of one KPM report
      kpm_du.c/.h          RNTI <-> CU UE ID, DRB.UEThpDl/Ul, RRU.PrbTotDl/Ul, DRB.RlcSduDelayDl
      kpm_cu.c/.h          per CU UE ID: DRB.PdcpSduVolumeDL/UL, amf_ue_ngap_id
    state/
      ue_table.c/.h        DU side: per DU and RNTI, 1 s windows, owns the one lock
      cu_table.c/.h        CU side: per CU UE ID (same lock)
      median.c/.h          median of the last MED_WINDOWS windows
      cell_view.c/.h       per DU: cell PRB use DL/UL (medians), UEs on the DU
      ue_view.c/.h         per CU UE ID: serving DU, RNTI re-map, handovers, active/idle, medians
    act/
      cmd_fifo.c/.h        command FIFO reader ("ho <CU UE ID> <DU number>", "show")
      act.c/.h             handover requests: check, send (lock released), confirm
    loop/
      tick.c/.h            the 1 s control loop (later: decide)
    output/
      csv.c/.h             main CSV file (one row per DU entry)
      csv_row.c/.h         the main CSV columns; joins the CU entry to the DU row by CU UE ID
      state_csv.c/.h       state CSV (one row per CU UE ID)
```

Later phases add `collect/rc_cu.c` (UE measurement reports, phase 1b), `act/`
(handover, core path switch), `decide/` (rules) and `core/` (free5GC / NEF client).

## Data flow

```
 DU1 / DU2 --E2--> near-RT RIC --E42--> e2/subscribe_du.c     e2/subscribe_cu.c <-- CU
                                            |                       |
                     collect/mac.c  rlc.c  kpm_du.c          collect/kpm_cu.c
                                            |                       |
                                  state/ue_table.c (RNTI)   state/cu_table.c (CU UE ID)
                                            |                       |
                                            +--- loop/tick.c (every full second) ---+
                                            |                       |               |
                                   output/csv.c          state/ue_view.c    state/cell_view.c
                                            |                       |               |
                             e2e_ctrl_<date>.csv          output/state_csv.c -------+
                                                                    |
                                                      e2e_ctrl_<date>_state.csv
```

Rule of thumb: `e2/` knows E2, `collect/` knows the report formats, `state/` knows
UEs and cells, `loop/` knows the clock, `output/` knows the files. A module only includes the headers of the modules
below it in this picture.

## Build (on the lab PC)

```bash
cd ~/e2e-ts-5g
cmake --build upstream/oai/openair2/E2AP/flexric/build --target e2e_ctrl -j4
```

The folder `Controller/xapp/e2e_ctrl` is symlinked into
`upstream/oai/openair2/E2AP/flexric/examples/xApp/c/e2e_ctrl`, and
`patches/flexric-add-e2e-ctrl.patch` adds `add_subdirectory(e2e_ctrl)` there.
After adding or renaming a `.c` file, list it in `CMakeLists.txt`.

## Run

```bash
cd ~/e2e-ts-5g
sudo ip netns exec e2e-ran \
  upstream/oai/openair2/E2AP/flexric/build/examples/xApp/c/e2e_ctrl/e2e_ctrl
```

Stops with Ctrl+C. CSV path: `$E2E_CSV`, else `e2e_ctrl_<date>_<time>.csv` in the
current folder. Note: with `sudo`, pass the variable as
`sudo E2E_CSV=... ip netns exec ...`.

## CSV columns

| Column | Source | Meaning |
|---|---|---|
| `t_unix`, `t_rel_s` | clock | end of the 1 s window; seconds since start |
| `du`, `nb_id`, `nr_cell_id` | E2 node, F1 Setup | which DU / cell |
| `rnti`, `cu_ue_id` | MAC/RLC, KPM | the DU's and the CU's UE identity |
| `dl_mbps`, `ul_mbps` | RLC | change of `txpdu_bytes` (DRB 1) / `rxpdu_bytes` (DRB 2) over DU time |
| `dl_mcs`, `ul_mcs`, `dl_bler`, `ul_bler`, `pusch_snr`, `pucch_snr` | MAC | mean of the ~100 reports in the window |
| `kpm_dl_thp_kbps`, `kpm_ul_thp_kbps`, `kpm_prb_dl`, `kpm_prb_ul` | KPM | last report in the window (UL throughput is known wrong, don't use) |
| `cell_prb_dl`, `cell_prb_ul` | KPM | sum of the UEs' PRB values on that DU |
| `n_mac`, `n_rlc`, `n_kpm`, `rlc_resets` | - | reports in the window; RLC counter resets |
| `rlc_delay_dl_us` | DU KPM | `DRB.RlcSduDelayDl`: mean time a DL packet waits in the RLC buffer (DRB 1) |
| `amf_ue_ngap_id` | CU KPM | the core's (AMF's) ID of this UE |
| `cu_pdcp_dl_mbit`, `cu_pdcp_ul_mbit` | CU KPM | `DRB.PdcpSduVolumeDL/UL`, whole Mbit per 1 s report, DRB 1 only (OAI) |
| `n_kpm_cu` | - | CU KPM reports in the window |
| `dl_hol_us_mean`, `dl_hol_us_max` | RLC | head-of-line delay on DRB 1 (`txsdu_wt_us`): how long the oldest queued DL packet has waited; mean / max of ~100 samples |
| `dl_buf_bytes_mean`, `dl_buf_bytes_max` | RLC | queued DL bytes on DRB 1 (`txbuf_occ_bytes`) |

Note on `rlc_delay_dl_us`: OAI resets the RLC delay average every time RLC stats are read
(`read_rlc_sm` -> `active_avg_to_tx` -> `time_average_reset`). With the RLC subscription at 10 ms
it only covers the last <=10 ms and is mostly 0. Use `dl_hol_us_*` instead.

## State CSV (phase 2)

Path: `$E2E_STATE_CSV`, else the main CSV path with `_state` before `.csv`.
One row per CU UE ID per second. Medians use the last `MED_WINDOWS` (5) windows.

| Column | Meaning |
|---|---|
| `t_unix`, `t_rel_s` | end of the 1 s window; seconds since start |
| `cu_ue_id`, `du`, `rnti` | the UE, its serving DU and its RNTI on that DU |
| `active` | 1 = RLC DL + UL >= `ACTIVE_MBPS`; 0 after `IDLE_WINDOWS` windows below |
| `ho_count`, `s_on_du` | serving-DU changes seen; seconds on the current DU |
| `dl_mbps_med`, `ul_mbps_med`, `kpm_dl_kbps_med` | UE traffic medians (kept after a handover) |
| `dl_mcs_med`, `ul_mcs_med`, `dl_bler_med`, `ul_bler_med`, `n_radio` | radio medians on the serving DU, only from windows with traffic; cleared after a handover; `n_radio` = windows used |
| `srv_prb_dl_med`, `srv_prb_ul_med`, `srv_n_ue` | serving cell: PRB use medians, UEs on the DU |
| `alt_du`, `alt_prb_ul_med`, `alt_n_ue` | other DU with the lowest UL PRB median |

How the view finds the serving DU: a DU entry is mapped to a UE by the CU UE ID in its
DU KPM report (needs `oai-kpm-du-rnti.patch`). After a handover the new RNTI is mapped
when its first KPM report arrives (up to 1 s). If two DU entries report the same CU UE
ID in one window, the entry with the newest report wins.

## Commands (phase 3)

The controller creates a FIFO (named pipe): `$E2E_CMD`, else `/tmp/e2e_ctrl.cmd`
(mode 0666, removed at stop). One command per line:

```bash
echo 'ho 1 2' > /tmp/e2e_ctrl.cmd    # hand CU UE ID 1 over to DU2
echo 'show'   > /tmp/e2e_ctrl.cmd    # print the UE view on stderr
```

A handover request is refused (one `[act] ... REFUSED: <reason>` line) when: the CU has
no RC handover control; the target DU is unknown; the UE is not known or did not report
in the last window; the UE is already on the target DU; a request for the UE is pending;
the last request is less than `HO_COOLDOWN_S` (5 s) old; there is no CU KPM report for
the UE yet. Otherwise the controller sends the CONTROL **without holding the UE table
lock** (FlexRIC waits for the CU's answer on the event loop that also delivers the
reports) and prints the answer time. The request is "confirmed" when the UE view shows
the target DU, or "NOT confirmed" after `HO_CONFIRM_S` (10 s).

`tools/ho-ctrl.sh <CU UE ID>` writes the command for "the other DU" and waits until the
UE is in-sync there. `HO_TOOL=tools/ho-ctrl.sh tools/hub-ho-test.sh --no-start` runs the
hub handover test through the controller instead of `xapp_rc_handover`.

