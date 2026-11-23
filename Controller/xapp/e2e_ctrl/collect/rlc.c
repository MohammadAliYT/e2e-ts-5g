/* collect/rlc.c - RLC reports -> per UE: DL/UL bytes and time, DL queue. See rlc.h.
 *
 * A UE has more than one DRB (the lab: 2 per PDU session), and the DRB that carries the DL or UL data can
 * change: on 23 Nov 2026, after a failed handover and an RRC re-establishment, the DL data left DRB 1 and the
 * controller read DL 0 while 10 Mbit/s arrived (results/reest/run-20261123-162439). So every UE value is
 * taken over all its DRBs:
 *   DL bytes  = sum of the txpdu_bytes changes     UL bytes              = sum of the rxpdu_bytes changes
 *   DL buffer = sum of txbuf_occ_bytes              DL head-of-line delay = max of txsdu_wt_us
 * PDU bytes include RLC headers, retransmissions and status PDUs (as before). */
#include "collect/rlc.h"

#include "state/ue_table.h"
#include "util/time_now_us.h"

/* Delta of a cumulative uint32 counter. A large backwards jump is a wrap-around (unsigned
 * arithmetic gives the right delta); a small one is a counter reset (new RLC entity). */
static bool counter_delta(uint32_t cur, uint32_t prev, uint32_t* delta)
{
  if (cur >= prev || prev - cur > 0x80000000u) {
    *delta = cur - prev;
    return true;
  }
  *delta = 0;
  return false;
}

/* Sums of one report for one UE (all its DRBs) */
typedef struct {
  ue_t* u;
  uint64_t dl_bytes, ul_bytes, buf_bytes;
  uint32_t hol_us;
} ue_sum_t;

/* Sum entry of u in s[0..*n-1], added if new. NULL if s is full. */
static ue_sum_t* sum_of(ue_sum_t s[], size_t* n, ue_t* u)
{
  for (size_t i = 0; i < *n; i++)
    if (s[i].u == u)
      return &s[i];
  if (*n >= MAX_UE)
    return NULL;
  s[*n] = (ue_sum_t){.u = u};
  return &s[(*n)++];
}

/* Add one bearer report to the sums of its UE. The first report of a bearer only sets its counter base. */
static void add_bearer(ue_sum_t* e, rlc_radio_bearer_stats_t const* rb)
{
  ue_t* u = e->u;
  int const r = rb->rbid;
  if (u->rlc_init[r]) {
    uint32_t d_tx, d_rx;
    bool const ok_tx = counter_delta(rb->txpdu_bytes, u->last_tx[r], &d_tx);
    bool const ok_rx = counter_delta(rb->rxpdu_bytes, u->last_rx[r], &d_rx);
    if (!ok_tx || !ok_rx)
      u->w_rlc_resets++;
    e->dl_bytes += d_tx;
    e->ul_bytes += d_rx;
  }
  u->rlc_init[r] = true;
  u->last_tx[r] = rb->txpdu_bytes;
  u->last_rx[r] = rb->rxpdu_bytes;
  e->buf_bytes += rb->txbuf_occ_bytes;
  if (rb->txsdu_wt_us > e->hol_us)
    e->hol_us = rb->txsdu_wt_us;
}

/* Close one report for one UE: a DL queue sample always; bytes and time from the UE's second report on.
 * A report with the same DU time as the last one adds no bytes (the counters still move on). */
static void add_report(ue_sum_t const* e, int64_t ts)
{
  ue_t* u = e->u;
  u->w_n_q++;
  u->w_dl_hol_us_sum += e->hol_us;
  u->w_dl_buf_bytes_sum += (double)e->buf_bytes;
  if (e->hol_us > u->w_dl_hol_us_max)
    u->w_dl_hol_us_max = e->hol_us;
  if (e->buf_bytes > u->w_dl_buf_bytes_max)
    u->w_dl_buf_bytes_max = e->buf_bytes > UINT32_MAX ? UINT32_MAX : (uint32_t)e->buf_bytes;

  if (u->rlc_ts_init) {
    int64_t const dt = ts - u->last_rlc_ts;
    if (dt > 0) {
      u->w_dl_bytes += e->dl_bytes;
      u->w_ul_bytes += e->ul_bytes;
      u->w_dl_dt_us += dt;
      u->w_ul_dt_us += dt;
      u->w_n_rlc++;
    }
  }
  u->rlc_ts_init = true;
  u->last_rlc_ts = ts;
}

void collect_rlc(size_t di, sm_ag_if_rd_t const* rd)
{
  if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0 || rd->ind.type != RLC_STATS_V0)
    return;

  rlc_ind_msg_t const* m = &rd->ind.rlc.msg;
  int64_t const now = time_now_us();
  int64_t const ts = m->tstamp; /* DU clock, time_now_us() in read_rlc_sm */
  ue_sum_t s[MAX_UE];
  size_t n = 0;

  ue_table_lock();
  for (uint32_t i = 0; i < m->len; i++) {
    rlc_radio_bearer_stats_t const* rb = &m->rb[i];
    if (rb->rbid < 1 || rb->rbid >= MAX_RB)
      continue;
    ue_t* u = ue_table_get(di, (uint16_t)rb->rnti);
    if (u == NULL)
      continue;
    u->last_seen_us = now;
    ue_sum_t* e = sum_of(s, &n, u);
    if (e != NULL)
      add_bearer(e, rb);
  }
  for (size_t k = 0; k < n; k++)
    add_report(&s[k], ts);
  ue_table_unlock();
}