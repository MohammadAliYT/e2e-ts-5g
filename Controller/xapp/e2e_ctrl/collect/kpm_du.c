/* collect/kpm_du.c - DU KPM style 4 reports -> UE table. See kpm_du.h. */
#include "collect/kpm_du.h"

#include <stdio.h>

#include "collect/kpm_values.h"
#include "state/ue_table.h"
#include "util/byte_array.h"
#include "util/time_now_us.h"

/* Store one named value in the window (last report wins) */
static void store(void* ctx, byte_array_t name, double v)
{
  ue_t* u = ctx;
  if (cmp_str_ba("DRB.UEThpDl", name) == 0)
    u->w_kpm_dl_thp = v;
  else if (cmp_str_ba("DRB.UEThpUl", name) == 0)
    u->w_kpm_ul_thp = v;
  else if (cmp_str_ba("RRU.PrbTotDl", name) == 0)
    u->w_kpm_prb_dl = v;
  else if (cmp_str_ba("RRU.PrbTotUl", name) == 0)
    u->w_kpm_prb_ul = v;
  else if (cmp_str_ba("DRB.RlcSduDelayDl", name) == 0)
    u->w_kpm_rlc_delay_dl_us = v;
}

void collect_kpm_du(size_t di, sm_ag_if_rd_t const* rd)
{
  if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0 || rd->ind.type != KPM_STATS_V3_0)
    return;

  kpm_ind_data_t const* ind = &rd->ind.kpm.ind;
  if (ind->msg.type != FORMAT_3_INDICATION_MESSAGE)
    return;

  kpm_ind_msg_format_3_t const* f3 = &ind->msg.frm_3;
  int64_t const now = time_now_us();

  ue_table_lock();
  for (size_t i = 0; i < f3->ue_meas_report_lst_len; i++) {
    meas_report_per_ue_t const* rep = &f3->meas_report_per_ue[i];
    ue_id_e2sm_t const* id = &rep->ue_meas_report_lst;
    /* DU KPM carries the RNTI in ran_ue_id only with oai-kpm-du-rnti.patch */
    if (id->type != GNB_DU_UE_ID_E2SM || id->gnb_du.ran_ue_id == NULL)
      continue;

    ue_t* u = ue_table_get(di, (uint16_t)*id->gnb_du.ran_ue_id);
    if (u == NULL)
      continue;
    u->last_seen_us = now;
    if (!u->has_cu_ue_id || u->cu_ue_id != id->gnb_du.gnb_cu_ue_f1ap)
      fprintf(stderr, "[e2e_ctrl] DU nb_id %u: RNTI %04x = CU UE ID %u\n", ue_table_nb_id(di), u->rnti,
              id->gnb_du.gnb_cu_ue_f1ap);
    u->has_cu_ue_id = true;
    u->cu_ue_id = id->gnb_du.gnb_cu_ue_f1ap;

    kpm_values_for_each(&rep->ind_msg_format_1, store, u);
    u->w_n_kpm++;
  }
  ue_table_unlock();
}
