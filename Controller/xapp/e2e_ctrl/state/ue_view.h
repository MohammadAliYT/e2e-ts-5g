/* state/ue_view.h - one entry per UE, keyed by the CU UE ID (phase 2 "state").
 *
 * The DU tables (state/ue_table.h) know a UE by (DU, RNTI). The RNTI changes after
 * every handover. The CU UE ID does not change after a handover (it changes after a
 * restart). Thus the controller decides per CU UE ID, and this view maps it to the
 * serving DU and the current RNTI.
 *
 * Every 1 s window:
 *   1. ue_view_begin_window()
 *   2. ue_view_add() for every DU entry that reported in the window (before ue_window_reset)
 *   3. ue_view_end_window(): find the serving DU, find handovers, update activity and medians
 * A DU entry without a CU UE ID (no KPM report yet, for example just after a handover)
 * is not mapped. It is mapped in a later window, when its first KPM report is in.
 *
 * Same lock as state/ue_table.h: hold ue_table_lock() while calling these.
 */
#ifndef E2E_CTRL_UE_VIEW_H
#define E2E_CTRL_UE_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "state/median.h"
#include "state/ue_table.h"

/* Values of one DU entry in one window (filled by ue_view_add) */
typedef struct {
  int di;              /* DU index, 0 = DU1 */
  uint16_t rnti;
  int64_t last_seen_us;
  bool has_dl, has_ul; /* RLC throughput known */
  double dl_mbps, ul_mbps;
  bool has_mac;
  double dl_mcs, ul_mcs, dl_bler, ul_bler; /* means of the window */
  bool has_kpm;
  double kpm_dl_kbps;
} ue_view_win_t;

typedef struct {
  bool used;
  uint32_t cu_ue_id;
  int64_t last_seen_us;

  /* where the UE is */
  int serving_di; /* -1 = not known yet */
  uint16_t rnti;  /* RNTI on the serving DU */
  uint32_t ho_count;   /* changes of the serving DU */
  uint32_t win_on_du;  /* windows on the current serving DU */

  /* activity: active when RLC DL + UL >= ACTIVE_MBPS; idle after IDLE_WINDOWS low windows */
  bool active;
  uint32_t low_windows;

  /* medians: UE traffic (kept after a handover) */
  med_t dl_mbps, ul_mbps, kpm_dl_kbps;
  /* medians: radio on the serving DU (only windows with traffic, cleared after a handover) */
  med_t dl_mcs, ul_mcs, dl_bler, ul_bler;

  /* this window */
  bool updated;     /* true after ue_view_end_window() if a DU entry reported */
  bool has_cand;    /* a DU entry was added in this window */
  ue_view_win_t cand;
} ue_view_t;

void ue_view_begin_window(void);

/* Add DU entry u of DU di. Ignored if u has no CU UE ID yet. If two DU entries
 * have the same CU UE ID (handover window), the entry with the newest report wins. */
void ue_view_add(size_t di, ue_t const* u);

/* Close the window. Prints a line on stderr for each event: new UE, handover,
 * RNTI change, active/idle change, UE gone (silent for more than UE_TIMEOUT_US). */
void ue_view_end_window(int64_t t_rel, int64_t now_us);

/* Slot i (0..MAX_UE-1), used or not - for loops over all UEs. */
ue_view_t const* ue_view_slot(size_t i);

/* Entry for a CU UE ID, or NULL. */
ue_view_t const* ue_view_find(uint32_t cu_ue_id);

#endif
