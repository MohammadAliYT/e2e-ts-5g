/* output/csv.c - the per-DU-entry CSV file. See csv.h. The 1 s clock is in loop/tick.c. */
#include "output/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "output/csv_row.h"

static FILE* g_csv;
static char g_path[256];

void csv_open(void)
{
  const char* path = getenv(CSV_PATH_ENV);
  if (path != NULL && path[0] != '\0') {
    snprintf(g_path, sizeof(g_path), "%s", path);
  } else {
    time_t const t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(g_path, sizeof(g_path), "e2e_ctrl_%Y%m%d_%H%M%S.csv", &tm);
  }
  g_csv = fopen(g_path, "w");
  if (g_csv == NULL) {
    perror(g_path);
    exit(EXIT_FAILURE);
  }
  csv_row_header(g_csv);
  fflush(g_csv);
  printf("[e2e_ctrl] CSV: %s\n", g_path);
}

const char* csv_path(void) { return g_path; }

void csv_write_ue(int64_t t_s, int64_t t_rel, size_t di, du_info_t const* du, ue_t const* u,
                  cell_window_t const* cell, cu_ue_t const* cu)
{
  csv_row_write(g_csv, t_s, t_rel, di + 1, du, u, cell, cu);
  csv_row_status(t_rel, di + 1, u, cu);
}

void csv_flush(void) { fflush(g_csv); }

void csv_close(void)
{
  if (g_csv != NULL)
    fclose(g_csv);
  g_csv = NULL;
}
