/* collect/kpm_cu.c - CU KPM style 4 reports -> CU table. See kpm_cu.h. */
#include "collect/kpm_cu.h"

#include "collect/kpm_values.h"
#include "state/cu_table.h"
#include "state/ue_table.h"
#include "util/byte_array.h"
#include "util/time_now_us.h"

static void store(void* ctx, byte_array_t name, double v)
{
  cu_ue_t* u = ctx;
  if (cmp_str_ba("DRB.PdcpSduVolumeDL", name) == 0)
    u->w_pdcp_dl_mbit = v;
  else if (cmp_str_ba("DRB.PdcpSduVolumeUL", name) == 0)
    u->w_pdcp_ul_mbit = v;
}

/* CU UE ID of a gNB-type UE ID: the F1AP list if present, else RAN UE ID (both = RRC UE ID in OAI) */
static bool cu_ue_id(gnb_e2sm_t const* g, uint32_t* id)
{
  if (g->gnb_cu_ue_f1ap_lst_len > 0 && g->gnb_cu_ue_f1ap_lst != NULL) {
    *id = g->gnb_cu_ue_f1ap_lst[0];
    return true;
  }
  if (g->ran_ue_id != NULL) {
    *id = (uint32_t)*g->ran_ue_id;
    return true;
  }
  return false;
}

void collect_kpm_cu(sm_ag_if_rd_t const* rd)
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
    uint32_t cu_id;
    if (id->type != GNB_UE_ID_E2SM || !cu_ue_id(&id->gnb, &cu_id))
      continue;
    cu_ue_t* u = cu_table_get(cu_id);
    if (u == NULL)
      continue;
    u->last_seen_us = now;
    u->amf_ue_ngap_id = id->gnb.amf_ue_ngap_id;
    cu_table_set_ue_id(u, id);
    kpm_values_for_each(&rep->ind_msg_format_1, store, u);
    u->w_n_kpm++;
  }
  ue_table_unlock();
}
