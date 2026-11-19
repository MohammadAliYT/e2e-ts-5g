# Controller

The E2E (end-to-end) traffic steering controller of the thesis. It runs as an **xApp**
on FlexRIC's near-RT RIC (RAN Intelligent Controller) and will later also talk to the
free5GC core.

Current state: **phase 0 - stage 1 "Observe"**: it reads MAC (Medium Access Control),
RLC (Radio Link Control) and KPM (Key Performance Measurement) reports from both DUs
(Distributed Units) and writes one CSV row per UE (User Equipment) per second.
It does not send anything back to the RAN yet.

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
      kpm_sub.c/.h         build the KPM REPORT style 4 subscription (per UE, slice filter)
      subscribe.c/.h       MAC + RLC + KPM subscriptions per DU, callbacks -> collect/
    collect/               one file per service model: E2 report -> UE table
      mac.c/.h             MCS, BLER, PUSCH/PUCCH SNR
      rlc.c/.h             byte counters -> DL bytes (DRB 1), UL bytes (DRB 2)
      kpm_du.c/.h          RNTI <-> CU UE ID, DRB.UEThpDl/Ul, RRU.PrbTotDl/Ul
    state/
      ue_table.c/.h        what is known per UE (per DU, per RNTI), 1 s windows, one lock
    output/
      csv.c/.h             writer thread: UE table -> CSV row per UE per second
```

Later phases add `collect/kpm_cu.c` (CU), `act/` (handover, core path switch),
`decide/` (rules) and `core/` (free5GC / NEF client).

## Data flow

```
 DU1 / DU2 --E2--> near-RT RIC --E42--> e2/subscribe.c (callback per DU)
                                            |
                         collect/mac.c  collect/rlc.c  collect/kpm_du.c
                                            |
                                   state/ue_table.c   (lock)
                                            |  every full second
                                   output/csv.c  -->  e2e_ctrl_<date>_<time>.csv
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
