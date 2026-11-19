/* collect/mac.c - MAC service model reports -> UE table. See mac.h. */
#include "collect/mac.h"

#include "state/ue_table.h"
#include "util/time_now_us.h"

void collect_mac(size_t di, sm_ag_if_rd_t const* rd)
{
  if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0 || rd->ind.type != MAC_STATS_V0)
    return;

  mac_ind_msg_t const* m = &rd->ind.mac.msg;
  int64_t const now = time_now_us();

  ue_table_lock();
  for (uint32_t i = 0; i < m->len_ue_stats; i++) {
    mac_ue_stats_impl_t const* s = &m->ue_stats[i];
    ue_t* u = ue_table_get(di, (uint16_t)s->rnti);
    if (u == NULL)
      continue;
    u->last_seen_us = now;
    u->w_dl_mcs += s->dl_mcs1;
    u->w_ul_mcs += s->ul_mcs1;
    u->w_dl_bler += s->dl_bler;
    u->w_ul_bler += s->ul_bler;
    u->w_pusch_snr += s->pusch_snr;
    u->w_pucch_snr += s->pucch_snr;
    u->w_n_mac++;
  }
  ue_table_unlock();
}
