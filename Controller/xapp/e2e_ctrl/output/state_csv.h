/* output/state_csv.h - the state CSV: one row per CU UE ID per second (phase 2).
 *
 * The rows show what a decision rule (phase 4) will see: serving DU, RNTI, activity,
 * handover count and the medians of the UE and of the cells.
 * Path: $E2E_STATE_CSV; else the main CSV path with "_state" before ".csv".
 * New columns go at the END.
 *
 * Columns:
 *   t_unix, t_rel_s, cu_ue_id, du, rnti, active, ho_count, s_on_du,
 *   dl_mbps_med, ul_mbps_med, kpm_dl_kbps_med,
 *   dl_mcs_med, ul_mcs_med, dl_bler_med, ul_bler_med, n_radio,
 *   srv_prb_dl_med, srv_prb_ul_med, srv_n_ue, alt_du, alt_prb_ul_med, alt_n_ue
 * "srv" = serving DU. "alt" = the other DU with the lowest UL PRB median (UL is the
 * capacity limit in this TDD pattern). n_radio = windows in the radio medians.
 */
#ifndef E2E_CTRL_STATE_CSV_H
#define E2E_CTRL_STATE_CSV_H

#include <stddef.h>
#include <stdint.h>

/* Call after csv_open(). Exits the program if the file cannot be created. */
void state_csv_open(void);

/* One row per UE updated in this window. n_du = number of DUs. Caller holds the lock. */
void state_csv_write(int64_t t_s, int64_t t_rel, size_t n_du);

void state_csv_flush(void);
void state_csv_close(void);

#endif
