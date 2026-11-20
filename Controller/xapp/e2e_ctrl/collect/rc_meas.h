/* collect/rc_meas.h - RC REPORT style 1 indications from the CU -> state/cu_table (phase 1b).
 *
 * Each indication is a copy of one UE MeasurementReport: RAN parameter 3 = the UL-DCCH message,
 * parameter 4 = the UE ID (gNB type, CU UE ID; needs oai-cu-rc-msg-copy-ueid.patch on the CU).
 * A copy without UE ID cannot be mapped to a UE and is only counted.
 */
#ifndef E2E_CTRL_COLLECT_RC_MEAS_H
#define E2E_CTRL_COLLECT_RC_MEAS_H

#include "xApp/e42_xapp_api.h"

/* Called from the E2 callback thread. */
void collect_rc_meas(sm_ag_if_rd_t const* rd);

#endif
