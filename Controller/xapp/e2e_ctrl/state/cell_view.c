/* state/cell_view.c - per-DU cell load with medians. See cell_view.h. */
#include "state/cell_view.h"

#include "config.h"
#include "state/ue_table.h"

static cell_t g_cell[MAX_DU];

cell_window_t cell_window_compute(size_t di)
{
  cell_window_t c = {0};
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_t const* u = ue_table_slot(di, i);
    if (u->used && u->w_n_kpm > 0) {
      c.prb_dl += u->w_kpm_prb_dl;
      c.prb_ul += u->w_kpm_prb_ul;
      c.valid = true;
    }
  }
  return c;
}

void cell_view_update(size_t di, cell_window_t const* w, uint32_t n_ue)
{
  if (di >= MAX_DU)
    return;
  cell_t* c = &g_cell[di];
  c->n_ue = n_ue;
  if (n_ue == 0) { /* empty cell: no load */
    med_push(&c->prb_dl, 0.0);
    med_push(&c->prb_ul, 0.0);
  } else if (w->valid) {
    med_push(&c->prb_dl, w->prb_dl);
    med_push(&c->prb_ul, w->prb_ul);
  } /* else: UEs but no KPM in this window -> no new value */
}

cell_t const* cell_view_get(size_t di) { return &g_cell[di < MAX_DU ? di : 0]; }
