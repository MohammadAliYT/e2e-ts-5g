/* output/csv.c - writer thread: UE table -> CSV every second. See csv.h. */
#include "output/csv.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "state/ue_table.h"
#include "util/time_now_us.h"

static const char CSV_HEADER[] =
    "t_unix,t_rel_s,du,nb_id,nr_cell_id,rnti,cu_ue_id,"
    "dl_mbps,ul_mbps,dl_mcs,ul_mcs,dl_bler,ul_bler,pusch_snr,pucch_snr,"
    "kpm_dl_thp_kbps,kpm_ul_thp_kbps,kpm_prb_dl,kpm_prb_ul,cell_prb_dl,cell_prb_ul,"
    "n_mac,n_rlc,n_kpm,rlc_resets\n";

static FILE* g_csv;
static int64_t g_t0_s;
static size_t g_n_du;
static du_info_t const* g_dus;
static atomic_bool g_stop;
static pthread_t g_thread;

/* ",value" or just "," when the value is missing */
static void csv_num(double v, bool valid, const char* fmt)
{
  fputc(',', g_csv);
  if (valid && isfinite(v))
    fprintf(g_csv, fmt, v);
}

static double mbps(uint64_t bytes, int64_t dt_us) { return dt_us > 0 ? (double)bytes * 8.0 / (double)dt_us : NAN; }

static void write_ue(int64_t t_s, size_t di, ue_t const* u, double cell_prb_dl, double cell_prb_ul, bool cell_kpm)
{
  du_info_t const* d = &g_dus[di];
  double const nm = u->w_n_mac;
  bool const mac = u->w_n_mac > 0;
  bool const kpm = u->w_n_kpm > 0;

  fprintf(g_csv, "%" PRId64 ",%" PRId64 ",%zu,%u,", t_s, t_s - g_t0_s, di + 1, d->nb_id);
  if (d->has_cell)
    fprintf(g_csv, "%" PRIu64, d->nr_cell_id);
  fprintf(g_csv, ",%04x,", u->rnti);
  if (u->has_cu_ue_id)
    fprintf(g_csv, "%u", u->cu_ue_id);

  csv_num(mbps(u->w_dl_bytes, u->w_dl_dt_us), u->w_dl_dt_us > 0, "%.3f");
  csv_num(mbps(u->w_ul_bytes, u->w_ul_dt_us), u->w_ul_dt_us > 0, "%.3f");
  csv_num(mac ? u->w_dl_mcs / nm : NAN, mac, "%.1f");
  csv_num(mac ? u->w_ul_mcs / nm : NAN, mac, "%.1f");
  csv_num(mac ? u->w_dl_bler / nm : NAN, mac, "%.4f");
  csv_num(mac ? u->w_ul_bler / nm : NAN, mac, "%.4f");
  csv_num(mac ? u->w_pusch_snr / nm : NAN, mac, "%.1f");
  csv_num(mac ? u->w_pucch_snr / nm : NAN, mac, "%.1f");
  csv_num(u->w_kpm_dl_thp, kpm, "%.0f");
  csv_num(u->w_kpm_ul_thp, kpm, "%.1f");
  csv_num(u->w_kpm_prb_dl, kpm, "%.0f");
  csv_num(u->w_kpm_prb_ul, kpm, "%.0f");
  csv_num(cell_prb_dl, cell_kpm, "%.0f");
  csv_num(cell_prb_ul, cell_kpm, "%.0f");
  fprintf(g_csv, ",%u,%u,%u,%u\n", u->w_n_mac, u->w_n_rlc, u->w_n_kpm, u->w_rlc_resets);

  fprintf(stderr, "[e2e_ctrl] t=%" PRId64 " DU%zu rnti=%04x dl=%.2f ul=%.2f Mbit/s dl_mcs=%.0f ul_mcs=%.0f\n",
          t_s - g_t0_s, di + 1, u->rnti,
          u->w_dl_dt_us > 0 ? mbps(u->w_dl_bytes, u->w_dl_dt_us) : 0.0,
          u->w_ul_dt_us > 0 ? mbps(u->w_ul_bytes, u->w_ul_dt_us) : 0.0,
          mac ? u->w_dl_mcs / nm : -1.0, mac ? u->w_ul_mcs / nm : -1.0);
}

/* One row per active UE of one DU; cell PRB = sum over the UEs with KPM in this window */
static void write_du(int64_t t_s, size_t di, int64_t now)
{
  double cell_prb_dl = 0, cell_prb_ul = 0;
  bool cell_kpm = false;
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_t const* u = ue_table_slot(di, i);
    if (u->used && u->w_n_kpm > 0) {
      cell_prb_dl += u->w_kpm_prb_dl;
      cell_prb_ul += u->w_kpm_prb_ul;
      cell_kpm = true;
    }
  }
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_t* u = ue_table_slot(di, i);
    if (!u->used || ue_window_idle(di, u, now))
      continue;
    write_ue(t_s, di, u, cell_prb_dl, cell_prb_ul, cell_kpm);
    ue_window_reset(u);
  }
}

static void write_window(int64_t t_s)
{
  int64_t const now = time_now_us();
  ue_table_lock();
  for (size_t di = 0; di < g_n_du; di++)
    write_du(t_s, di, now);
  ue_table_unlock();
  fflush(g_csv);
}

static void* writer_thread(void* arg)
{
  (void)arg;
  struct timespec next;
  clock_gettime(CLOCK_REALTIME, &next);
  next.tv_sec += 1;
  next.tv_nsec = 0;
  g_t0_s = next.tv_sec - 1;

  while (!atomic_load(&g_stop)) {
    while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL) == EINTR)
      ;
    write_window(next.tv_sec);
    next.tv_sec += 1;
  }
  return NULL;
}

void csv_open(void)
{
  char def[64];
  const char* path = getenv(CSV_PATH_ENV);
  if (path == NULL || path[0] == '\0') {
    time_t const t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(def, sizeof(def), "e2e_ctrl_%Y%m%d_%H%M%S.csv", &tm);
    path = def;
  }
  g_csv = fopen(path, "w");
  if (g_csv == NULL) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  fputs(CSV_HEADER, g_csv);
  fflush(g_csv);
  printf("[e2e_ctrl] CSV: %s\n", path);
}

void csv_start(size_t n_du, du_info_t const* dus)
{
  g_n_du = n_du;
  g_dus = dus;
  int const rc = pthread_create(&g_thread, NULL, writer_thread, NULL);
  assert(rc == 0);
  (void)rc;
}

void csv_stop(void)
{
  atomic_store(&g_stop, true);
  pthread_join(g_thread, NULL);
  fclose(g_csv);
}
