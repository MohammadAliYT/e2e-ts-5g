/* collect/cu_ue_id.c - CU UE ID from a gNB-type UE ID. See cu_ue_id.h. */
#include "collect/cu_ue_id.h"

bool cu_ue_id_of(gnb_e2sm_t const* g, uint32_t* id)
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
