/* output/state_csv.c - state CSV, one row per CU UE ID per second. See state_csv.h. */
#include "output/state_csv.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "output/csv.h"
#include "state/cell_view.h"
#include "state/ue_view.h"

static FILE* g_f;

static const char HEADER[] =
    "t_unix,t_rel_s,cu_ue_id,du,rnti,active,ho_count,s_on_du,"
    "dl_mbps_med,ul_mbps_med,kpm_dl_kbps_med,"
    "dl_mcs_med,ul_mcs_med,dl_bler_med,ul_bler_med,n_radio,"
    "srv_prb_dl_med,srv_prb_ul_med,srv_n_ue,alt_du,alt_prb_ul_med,alt_n_ue\n";

/* Main CSV path "x.csv" -> "x_state.csv"; without ".csv" -> append "_state.csv" */
static void derive_path(char* out, size_t len)
{
  const char* env = getenv(STATE_CSV_PATH_ENV);
  if (env != NULL && env[0] != '\0') {
    snprintf(out, len, "%s", env);
    return;
  }
  const char* main_path = csv_path();
  size_t n = strlen(main_path);
  if (n >= 4 && strcmp(main_path + n - 4, ".csv") == 0)
    n -= 4;
  snprintf(out, len, "%.*s_state.csv", (int)n, main_path);
}

void state_csv_open(void)
{
  char path[300];
  derive_path(path, sizeof(path));
  g_f = fopen(path, "w");
  if (g_f == NULL) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  fputs(HEADER, g_f);
  fflush(g_f);
  printf("[e2e_ctrl] state CSV: %s\n", path);
}

/* ",median" or just "," when there is no value */
static void med(FILE* f, med_t const* m, const char* fmt)
{
  double v;
  fputc(',', f);
  if (med_get(m, &v) && isfinite(v))
    fprintf(f, fmt, v);
}

/* The other DU with the lowest UL PRB median, or -1 */
static int alt_du(int serving, size_t n_du)
{
  int best = -1;
  double best_v = INFINITY;
  for (size_t dj = 0; dj < n_du; dj++) {
    double v;
    if ((int)dj == serving || !med_get(&cell_view_get(dj)->prb_ul, &v))
      continue;
    if (v < best_v) {
      best_v = v;
      best = (int)dj;
    }
  }
  return best;
}

static void write_row(int64_t t_s, int64_t t_rel, ue_view_t const* v, size_t n_du)
{
  fprintf(g_f, "%" PRId64 ",%" PRId64 ",%u,%d,%04x,%d,%u,%u", t_s, t_rel, v->cu_ue_id, v->serving_di + 1, v->rnti,
          v->active ? 1 : 0, v->ho_count, v->win_on_du);
  med(g_f, &v->dl_mbps, "%.3f");
  med(g_f, &v->ul_mbps, "%.3f");
  med(g_f, &v->kpm_dl_kbps, "%.0f");
  med(g_f, &v->dl_mcs, "%.1f");
  med(g_f, &v->ul_mcs, "%.1f");
  med(g_f, &v->dl_bler, "%.4f");
  med(g_f, &v->ul_bler, "%.4f");
  fprintf(g_f, ",%u", v->dl_mcs.n);

  cell_t const* srv = cell_view_get((size_t)v->serving_di);
  med(g_f, &srv->prb_dl, "%.0f");
  med(g_f, &srv->prb_ul, "%.0f");
  fprintf(g_f, ",%u", srv->n_ue);

  int const a = alt_du(v->serving_di, n_du);
  if (a < 0) {
    fputs(",,,", g_f);
  } else {
    cell_t const* alt = cell_view_get((size_t)a);
    fprintf(g_f, ",%d", a + 1);
    med(g_f, &alt->prb_ul, "%.0f");
    fprintf(g_f, ",%u", alt->n_ue);
  }
  fputc('\n', g_f);
}

void state_csv_write(int64_t t_s, int64_t t_rel, size_t n_du)
{
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_view_t const* v = ue_view_slot(i);
    if (v->used && v->updated && v->serving_di >= 0)
      write_row(t_s, t_rel, v, n_du);
  }
}

void state_csv_flush(void) { fflush(g_f); }

void state_csv_close(void)
{
  if (g_f != NULL)
    fclose(g_f);
  g_f = NULL;
}
