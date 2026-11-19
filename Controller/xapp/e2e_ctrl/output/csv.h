/* output/csv.h - one CSV row per UE per second, plus a short status line on stderr.
 *
 * A writer thread wakes at every full wall-clock second, turns each UE's 1 s window
 * into a row (Mbit/s from RLC bytes, MAC averages, last KPM values, cell PRB = sum
 * of the UEs' KPM PRB values), and starts a new window.
 * Columns: see CSV_HEADER in csv.c.
 */
#ifndef E2E_CTRL_CSV_H
#define E2E_CTRL_CSV_H

#include <stddef.h>

#include "e2/nodes.h"

/* Opens the CSV (path from $E2E_CSV, else e2e_ctrl_<date>_<time>.csv) and writes the header.
 * Exits the program if the file cannot be created. */
void csv_open(void);

/* Starts the writer thread. dus must stay valid until csv_stop(). */
void csv_start(size_t n_du, du_info_t const* dus);

/* Stops the writer thread and closes the file. */
void csv_stop(void);

#endif
