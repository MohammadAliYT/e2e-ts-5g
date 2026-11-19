/* output/csv.c - CSV file + writer thread (1 s windows). See csv.h. */
#include "output/csv.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "output/csv_row.h"
#include "state/cu_table.h"
#include "state/ue_table.h"
#include "util/time_now_us.h"

static FILE* g_csv;
static int64_t g_t0_s;
static ran_nodes_t const* g_ran;
static atomic_bool g_stop;
static pthread_t g_thread;

/* Cell PRB of DU di = sum over its UEs with KPM in this window */
static cell_window_t cell_window(size_t di)
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

/* One row per active UE of DU di */
static void write_du(int64_t t_s, size_t di, int64_t now)
{
  cell_window_t const cell = cell_window(di);
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_t* u = ue_table_slot(di, i);
    if (!u->used || ue_window_idle(di, u, now))
      continue;
    cu_ue_t const* cu = u->has_cu_ue_id ? cu_table_find(u->cu_ue_id) : NULL;
    csv_row_write(g_csv, t_s, t_s - g_t0_s, di + 1, &g_ran->du[di], u, &cell, cu);
    csv_row_status(t_s - g_t0_s, di + 1, u, cu);
    ue_window_reset(u);
  }
}

static void write_window(int64_t t_s)
{
  int64_t const now = time_now_us();
  ue_table_lock();
  for (size_t di = 0; di < g_ran->n_du; di++)
    write_du(t_s, di, now);
  cu_table_new_window(now);
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
  csv_row_header(g_csv);
  fflush(g_csv);
  printf("[e2e_ctrl] CSV: %s\n", path);
}

void csv_start(ran_nodes_t const* ran)
{
  g_ran = ran;
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
