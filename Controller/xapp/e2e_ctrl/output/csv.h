/* output/csv.h - the CSV file and its once-per-second writer thread.
 *
 * At every full wall-clock second the writer takes the lock, writes one row per active
 * UE of every DU (with the CU values of the same CU UE ID joined in), and starts a new
 * window in both tables. Column layout: output/csv_row.c.
 */
#ifndef E2E_CTRL_CSV_H
#define E2E_CTRL_CSV_H

#include "e2/nodes.h"

/* Opens the CSV (path from $E2E_CSV, else e2e_ctrl_<date>_<time>.csv) and writes the header.
 * Exits the program if the file cannot be created. */
void csv_open(void);

/* Starts the writer thread. ran must stay valid until csv_stop(). */
void csv_start(ran_nodes_t const* ran);

/* Stops the writer thread and closes the file. */
void csv_stop(void);

#endif
