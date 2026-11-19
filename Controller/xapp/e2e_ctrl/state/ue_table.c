/* state/ue_table.c - per-DU UE tables, one lock for all of them. See ue_table.h. */
#include "state/ue_table.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static ue_t g_ue[MAX_DU][MAX_UE];
static uint32_t g_nb_id[MAX_DU];
static size_t g_n_du;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

void ue_table_init(size_t n_du, uint32_t const nb_id[])
{
  g_n_du = n_du < MAX_DU ? n_du : MAX_DU;
  for (size_t di = 0; di < g_n_du; di++)
    g_nb_id[di] = nb_id[di];
}

size_t ue_table_n_du(void) { return g_n_du; }
uint32_t ue_table_nb_id(size_t di) { return g_nb_id[di]; }

void ue_table_lock(void) { pthread_mutex_lock(&g_mtx); }
void ue_table_unlock(void) { pthread_mutex_unlock(&g_mtx); }

ue_t* ue_table_get(size_t di, uint16_t rnti)
{
  ue_t* free_slot = NULL;
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_t* u = &g_ue[di][i];
    if (u->used && u->rnti == rnti)
      return u;
    if (!u->used && free_slot == NULL)
      free_slot = u;
  }
  if (free_slot == NULL) {
    fprintf(stderr, "[e2e_ctrl] DU nb_id %u: UE table full, RNTI %04x ignored\n", g_nb_id[di], rnti);
    return NULL;
  }
  memset(free_slot, 0, sizeof(*free_slot));
  free_slot->used = true;
  free_slot->rnti = rnti;
  fprintf(stderr, "[e2e_ctrl] DU nb_id %u: new UE RNTI %04x\n", g_nb_id[di], rnti);
  return free_slot;
}

ue_t* ue_table_slot(size_t di, size_t i) { return &g_ue[di][i]; }

bool ue_window_idle(size_t di, ue_t* u, int64_t now_us)
{
  /* UL RLC reports do not count in w_n_rlc, so check w_ul_dt_us as well */
  if (u->w_n_mac > 0 || u->w_n_rlc > 0 || u->w_n_kpm > 0 || u->w_ul_dt_us > 0 || u->w_n_q > 0)
    return false;
  if (now_us - u->last_seen_us > UE_TIMEOUT_US) {
    fprintf(stderr, "[e2e_ctrl] DU nb_id %u: UE RNTI %04x gone\n", g_nb_id[di], u->rnti);
    u->used = false;
  }
  return true;
}

void ue_window_reset(ue_t* u)
{
  u->w_dl_bytes = u->w_ul_bytes = 0;
  u->w_dl_dt_us = u->w_ul_dt_us = 0;
  u->w_n_rlc = u->w_rlc_resets = 0;
  u->w_n_q = 0;
  u->w_dl_hol_us_sum = u->w_dl_buf_bytes_sum = 0;
  u->w_dl_hol_us_max = u->w_dl_buf_bytes_max = 0;
  u->w_dl_mcs = u->w_ul_mcs = u->w_dl_bler = u->w_ul_bler = u->w_pusch_snr = u->w_pucch_snr = 0;
  u->w_n_mac = 0;
  u->w_n_kpm = 0;
  u->w_kpm_dl_thp = u->w_kpm_ul_thp = u->w_kpm_prb_dl = u->w_kpm_prb_ul = 0;
  u->w_kpm_rlc_delay_dl_us = 0;
}
