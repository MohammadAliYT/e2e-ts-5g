/* collect/cu_ue_id.h - the CU UE ID inside a gNB-type E2SM UE ID.
 *
 * OAI's CU fills the gNB-CU UE F1AP ID list and the RAN UE ID with the same number, its RRC UE ID
 * (ran_e2sm_ue_id.c, fill_e2sm_cu_ue_id_data). Used by collect/kpm_cu.c and collect/rc_meas.c.
 */
#ifndef E2E_CTRL_COLLECT_CU_UE_ID_H
#define E2E_CTRL_COLLECT_CU_UE_ID_H

#include <stdbool.h>
#include <stdint.h>

#include "lib/sm/ie/ue_id.h"

/* The F1AP list if present, else the RAN UE ID. False if the UE ID has neither. */
bool cu_ue_id_of(gnb_e2sm_t const* g, uint32_t* id);

#endif
