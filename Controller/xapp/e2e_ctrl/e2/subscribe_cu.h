/* e2/subscribe_cu.h - E2 REPORT subscriptions on the CU.
 *
 * Phase 1a: KPM style 4 (per-UE PDCP volume DL/UL + the UE's core ID).
 * Phase 1b: RC style 1 (a copy of every UE MeasurementReport, with the UE ID).
 * SAFETY: the GTP (148) and PDCP (144) service models are never subscribed on the
 * CU - GTP crashed the CU on 19 Nov (see config.h). subscribe_cu.c refuses them.
 */
#ifndef E2E_CTRL_SUBSCRIBE_CU_H
#define E2E_CTRL_SUBSCRIBE_CU_H

#include "e2/nodes.h"
#include "xApp/e42_xapp_api.h"

typedef struct {
  sm_ans_xapp_t kpm;     /* .success false = not subscribed */
  sm_ans_xapp_t rc_meas;
} cu_subs_t;

void subscribe_cu(cu_info_t const* cu, cu_subs_t* subs);
void unsubscribe_cu(cu_subs_t const* subs);

#endif
