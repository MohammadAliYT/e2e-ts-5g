/* collect/kpm_du.h - KPM REPORT style 4 (per UE) from a DU -> UE table.
 *
 * Gives the UE's CU UE ID (with oai-kpm-du-rnti.patch the RNTI is in ran_ue_id),
 * DRB.UEThpDl/Ul (kbps), RRU.PrbTotDl/Ul (% of cell PRBs) and DRB.RlcSduDelayDl
 * (us, DRB 1 only - OAI ran_func_kpm_subs.c). These are all 5 measurements the OAI
 * w46 DU offers for style 4.
 */
#ifndef E2E_CTRL_COLLECT_KPM_DU_H
#define E2E_CTRL_COLLECT_KPM_DU_H

#include <stddef.h>

#include "xApp/e42_xapp_api.h"

/* di = DU index (0 = DU1). Called from the E2 callback thread. */
void collect_kpm_du(size_t di, sm_ag_if_rd_t const* rd);

#endif
