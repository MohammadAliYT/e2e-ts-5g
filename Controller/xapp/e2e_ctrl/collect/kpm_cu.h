/* collect/kpm_cu.h - KPM REPORT style 4 (per UE) from the CU -> state/cu_table.
 *
 * OAI w46 gNB-CU offers DRB.PdcpSduVolumeDL / DRB.PdcpSduVolumeUL: whole Mbit per
 * report period, read from DRB 1 only (ran_func_kpm_subs.c). The UE ID is of type
 * gNB: CU UE ID (gnb_cu_ue_f1ap_lst[0], = RRC UE ID) and amf_ue_ngap_id (core side).
 */
#ifndef E2E_CTRL_COLLECT_KPM_CU_H
#define E2E_CTRL_COLLECT_KPM_CU_H

#include "xApp/e42_xapp_api.h"

/* Called from the E2 callback thread. */
void collect_kpm_cu(sm_ag_if_rd_t const* rd);

#endif
