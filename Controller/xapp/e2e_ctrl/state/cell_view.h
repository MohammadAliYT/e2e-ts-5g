/* state/cell_view.h - what the controller knows about each cell (one cell per DU).
 *
 * Cell load = sum of the KPM RRU.PrbTotDl/Ul values of the UEs on that DU.
 * The view keeps the median of the last MED_WINDOWS windows.
 * A DU with no UE has 0 % PRB use (known value). A DU with UEs but no KPM report
 * in the window gives no value (the median does not change).
 * Same lock as state/ue_table.h: hold ue_table_lock() while calling these.
 */
#ifndef E2E_CTRL_CELL_VIEW_H
#define E2E_CTRL_CELL_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "state/median.h"

/* Cell values of one DU in one window */
typedef struct {
  double prb_dl, prb_ul; /* sum of the UEs' KPM PRB values, % */
  bool valid;            /* at least one UE had KPM in this window */
} cell_window_t;

typedef struct {
  med_t prb_dl, prb_ul; /* medians of cell_window_t values */
  uint32_t n_ue;        /* UEs that reported in the last window */
} cell_t;

/* Cell values of DU di in the current window, from the UE table.
 * Call before ue_window_reset(). Caller holds the lock. */
cell_window_t cell_window_compute(size_t di);

/* Close the window of DU di: n_ue = UEs that reported in this window. */
void cell_view_update(size_t di, cell_window_t const* w, uint32_t n_ue);

/* View of DU di (0 = DU1). */
cell_t const* cell_view_get(size_t di);

#endif
