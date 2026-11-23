/* state/ue_table.h - what the controller currently knows about each UE.
 *
 * One table per DU, one entry per RNTI (the DU's name for a UE). The collect/
 * modules write into it from the E2 callbacks; output/csv.c reads it once per
 * second. Everything is protected by one lock: call ue_table_lock() before
 * touching any ue_t and ue_table_unlock() after.
 */
#ifndef E2E_CTRL_UE_TABLE_H
#define E2E_CTRL_UE_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

typedef struct {
  bool used;
  uint16_t rnti;
  int64_t last_seen_us;

  /* identity from KPM: the CU's UE ID (survives handovers, not restarts) */
  bool has_cu_ue_id;
  uint32_t cu_ue_id;

  /* RLC: last cumulative byte counters per DRB; DU time of the last report (all DRBs) */
  bool rlc_init[MAX_RB];
  uint32_t last_tx[MAX_RB];
  uint32_t last_rx[MAX_RB];
  bool rlc_ts_init;
  int64_t last_rlc_ts;

  /* ---- accumulators of the current 1 s window (reset by ue_window_reset) ---- */
  uint64_t w_dl_bytes, w_ul_bytes;
  int64_t w_dl_dt_us, w_ul_dt_us;
  uint32_t w_n_rlc;      /* RLC reports (one per report and UE, all DRBs) */
  uint32_t w_rlc_resets; /* counter resets seen (new RLC entity) */

  /* DL queue on DRB 1, sampled with every RLC report (~100 per window) */
  uint32_t w_n_q;
  /* DL queue over all DRBs, sampled with every RLC report (~100 per window) */
  double w_dl_hol_us_sum, w_dl_buf_bytes_sum; /* head-of-line delay (max txsdu_wt_us), queued bytes (sum) */
  uint32_t w_dl_hol_us_max, w_dl_buf_bytes_max;

  double w_dl_mcs, w_ul_mcs, w_dl_bler, w_ul_bler, w_pusch_snr, w_pucch_snr; /* sums */
  uint32_t w_n_mac;

  uint32_t w_n_kpm;
  double w_kpm_dl_thp, w_kpm_ul_thp; /* kbps, last report in window */
  double w_kpm_prb_dl, w_kpm_prb_ul; /* % of cell PRBs, last report in window */
  double w_kpm_rlc_delay_dl_us;      /* DRB.RlcSduDelayDl: mean DL wait in the RLC buffer, us */
} ue_t;

/* Call once before any E2 subscription: number of DUs and their nb_id (for log lines). */
void ue_table_init(size_t n_du, uint32_t const nb_id[]);
size_t ue_table_n_du(void);
uint32_t ue_table_nb_id(size_t di);

void ue_table_lock(void);
void ue_table_unlock(void);

/* Entry for (DU di, RNTI); created if new. NULL if the table is full. Caller holds the lock. */
ue_t* ue_table_get(size_t di, uint16_t rnti);

/* Slot i (0..MAX_UE-1) of DU di, used or not - for loops over all UEs. Caller holds the lock. */
ue_t* ue_table_slot(size_t di, size_t i);

/* True if u received no report in the current window. Frees the entry when it has been
 * silent for more than UE_TIMEOUT_US. Caller holds the lock. */
bool ue_window_idle(size_t di, ue_t* u, int64_t now_us);

/* Start a new 1 s window for u. Caller holds the lock. */
void ue_window_reset(ue_t* u);

#endif
