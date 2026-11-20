/* output/csv_row.h - the CSV columns: header and one row per UE and second.
 *
 * Only this file knows the column order. New columns go at the END, so scripts that
 * cut columns by number keep working.
 */
#ifndef E2E_CTRL_CSV_ROW_H
#define E2E_CTRL_CSV_ROW_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "e2/nodes.h"
#include "state/cell_view.h" /* cell_window_t */
#include "state/cu_table.h"
#include "state/ue_table.h"

void csv_row_header(FILE* f);

/* One row. du_no = 1 for DU1. cu may be NULL (no CU report for this UE). t_rel = seconds since start. */
void csv_row_write(FILE* f, int64_t t_s, int64_t t_rel, size_t du_no, du_info_t const* du, ue_t const* u,
                   cell_window_t const* cell, cu_ue_t const* cu);

/* Short status line for the same UE on stderr */
void csv_row_status(int64_t t_rel, size_t du_no, ue_t const* u, cu_ue_t const* cu);

#endif
