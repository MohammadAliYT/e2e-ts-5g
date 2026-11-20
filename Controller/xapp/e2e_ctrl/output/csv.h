/* output/csv.h - the per-DU-entry CSV file (one row per (DU, RNTI) per second).
 *
 * loop/tick.c calls csv_write_ue() once per active DU entry in every 1 s window.
 * Column layout: output/csv_row.c.
 */
#ifndef E2E_CTRL_CSV_H
#define E2E_CTRL_CSV_H

#include <stddef.h>
#include <stdint.h>

#include "e2/nodes.h"
#include "state/cell_view.h"
#include "state/cu_table.h"
#include "state/ue_table.h"

/* Opens the CSV (path from $E2E_CSV, else e2e_ctrl_<date>_<time>.csv) and writes the header.
 * Exits the program if the file cannot be created. */
void csv_open(void);

/* Path of the open CSV (for output/state_csv.c, which derives its own path from it). */
const char* csv_path(void);

/* One row plus one status line on stderr. di = DU index (0 = DU1). cu may be NULL. */
void csv_write_ue(int64_t t_s, int64_t t_rel, size_t di, du_info_t const* du, ue_t const* u,
                  cell_window_t const* cell, cu_ue_t const* cu);

void csv_flush(void);
void csv_close(void);

#endif
