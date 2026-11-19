/* collect/rlc.c - RLC byte counters -> DL/UL bytes and time per window. See rlc.h. */
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

/* DL queue sample (every report, also the first one) */
static void add_queue_sample(ue_t* u, rlc_radio_bearer_stats_t const* rb)
{
  u->w_n_q++;
  u->w_dl_hol_us_sum += rb->txsdu_wt_us;
  u->w_dl_buf_bytes_sum += rb->txbuf_occ_bytes;
  if (rb->txsdu_wt_us > u->w_dl_hol_us_max)
    u->w_dl_hol_us_max = rb->txsdu_wt_us;
  if (rb->txbuf_occ_bytes > u->w_dl_buf_bytes_max)
    u->w_dl_buf_bytes_max = rb->txbuf_occ_bytes;
}

/* One bearer report for one UE */
static void add_bearer(ue_t* u, rlc_radio_bearer_stats_t const* rb, int64_t ts)
{
  int const r = rb->rbid;
  if (!u->rlc_init[r]) { /* first report: remember the counters, nothing to add yet */
    u->rlc_init[r] = true;
    u->last_tx[r] = rb->txpdu_bytes;
    u->last_rx[r] = rb->rxpdu_bytes;
    u->last_rlc_ts[r] = ts;
    return;
  }

  int64_t const dt = ts - u->last_rlc_ts[r];
  if (dt <= 0)
    return;

  uint32_t d_tx, d_rx;
  bool const ok_tx = counter_delta(rb->txpdu_bytes, u->last_tx[r], &d_tx);
  bool const ok_rx = counter_delta(rb->rxpdu_bytes, u->last_rx[r], &d_rx);
  if (!ok_tx || !ok_rx)
    u->w_rlc_resets++;

  if (r == DL_DRB) {
    u->w_dl_bytes += d_tx;
    u->w_dl_dt_us += dt;
    u->w_n_rlc++;
  }
  if (r == UL_DRB) {
    u->w_ul_bytes += d_rx;
    u->w_ul_dt_us += dt;
  }

  u->last_tx[r] = rb->txpdu_bytes;
  u->last_rx[r] = rb->rxpdu_bytes;
  u->last_rlc_ts[r] = ts;
}

void collect_rlc(size_t di, sm_ag_if_rd_t const* rd)
{
  if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0 || rd->ind.type != RLC_STATS_V0)
    return;

  rlc_ind_msg_t const* m = &rd->ind.rlc.msg;
  int64_t const now = time_now_us();
  int64_t const ts = m->tstamp; /* DU clock, time_now_us() in read_rlc_sm */

  ue_table_lock();
  for (uint32_t i = 0; i < m->len; i++) {
    rlc_radio_bearer_stats_t const* rb = &m->rb[i];
    if (rb->rbid >= MAX_RB)
      continue;
    ue_t* u = ue_table_get(di, (uint16_t)rb->rnti);
    if (u == NULL)
      continue;
    u->last_seen_us = now;
    if (rb->rbid == DL_DRB)
      add_queue_sample(u, rb);
    add_bearer(u, rb, ts);
  }
  ue_table_unlock();
}
