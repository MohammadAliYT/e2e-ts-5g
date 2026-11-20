/* loop/tick.c - the 1 s control loop. See tick.h. */
#include "loop/tick.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#include "act/act.h"
#include "config.h"
#include "output/csv.h"
#include "output/state_csv.h"
#include "state/cell_view.h"
#include "state/cu_table.h"
#include "state/ue_table.h"
#include "state/ue_view.h"
#include "util/time_now_us.h"

static int64_t g_t0_s;
static ran_nodes_t const* g_ran;
static atomic_bool g_stop;
static pthread_t g_thread;

/* Close the window of every DU entry of DU di */
static void close_du(int64_t t_s, int64_t t_rel, size_t di, int64_t now)
{
  cell_window_t const cell = cell_window_compute(di);
  uint32_t n_ue = 0;
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_t* u = ue_table_slot(di, i);
    if (!u->used || ue_window_idle(di, u, now))
      continue;
    n_ue++;
    cu_ue_t const* cu = u->has_cu_ue_id ? cu_table_find(u->cu_ue_id) : NULL;
    csv_write_ue(t_s, t_rel, di, &g_ran->du[di], u, &cell, cu);
    ue_view_add(di, u); /* before the reset */
    ue_window_reset(u);
  }
  cell_view_update(di, &cell, n_ue);
}

static void close_window(int64_t t_s)
{
  int64_t const now = time_now_us();
  int64_t const t_rel = t_s - g_t0_s;

  ue_table_lock();
  ue_view_begin_window();
  for (size_t di = 0; di < g_ran->n_du; di++)
    close_du(t_s, t_rel, di, now);
  cu_table_new_window(now);
  ue_view_end_window(t_rel, now);
  act_check_pending(t_rel, now);
  state_csv_write(t_s, t_rel, g_ran->n_du);
  ue_table_unlock();

  csv_flush();
  state_csv_flush();

  /* Act without the lock: a handover CONTROL blocks until the CU answers,
   * and the E2 report callbacks need the lock meanwhile */
  act_run_commands(t_rel);
}

static void* tick_thread(void* arg)
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
    close_window(next.tv_sec);
    next.tv_sec += 1;
  }
  return NULL;
}

void tick_start(ran_nodes_t const* ran)
{
  g_ran = ran;
  int const rc = pthread_create(&g_thread, NULL, tick_thread, NULL);
  assert(rc == 0);
  (void)rc;
}

void tick_stop(void)
{
  atomic_store(&g_stop, true);
  pthread_join(g_thread, NULL);
}
