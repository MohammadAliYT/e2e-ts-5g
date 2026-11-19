# Controller

The E2E (end-to-end) traffic steering controller of the thesis. It runs as an **xApp**
on FlexRIC's near-RT RIC (RAN Intelligent Controller) and will later also talk to the
free5GC core.

Current state: **phase 1a "Observe RAN"**: it reads MAC (Medium Access Control),
RLC (Radio Link Control) and KPM (Key Performance Measurement) reports from both DUs
(Distributed Units) and KPM from the CU (Central Unit), and writes one CSV row per
UE (User Equipment) per second. It does not send anything back to the RAN yet.

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
    collect/               one file per report type: E2 report -> state
      mac.c/.h             MCS, BLER, PUSCH/PUCCH SNR
      rlc.c/.h             byte counters -> DL bytes (DRB 1), UL bytes (DRB 2)
      kpm_values.c/.h      walks the (name, value) pairs of one KPM report
      kpm_du.c/.h          RNTI <-> CU UE ID, DRB.UEThpDl/Ul, RRU.PrbTotDl/Ul, DRB.RlcSduDelayDl
      kpm_cu.c/.h          per CU UE ID: DRB.PdcpSduVolumeDL/UL, amf_ue_ngap_id
    state/
      ue_table.c/.h        DU side: per DU and RNTI, 1 s windows, owns the one lock
      cu_table.c/.h        CU side: per CU UE ID (same lock)
    output/
      csv.c/.h             CSV file + writer thread (every full second)
      csv_row.c/.h         the columns; joins the CU entry to the DU row by CU UE ID
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
                                            |   every full second   |
                                            +---- output/csv.c -----+  joined by CU UE ID
                                                       |
                                          e2e_ctrl_<date>_<time>.csv
```

Rule of thumb: `e2/` knows E2, `collect/` knows the report formats, `state/` knows
UEs, `output/` knows the file. A module only includes the headers of the modules
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
