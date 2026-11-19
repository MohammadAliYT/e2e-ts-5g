/* collect/mac.h - MAC service model (ID 142) reports from a DU -> UE table.
 *
 * Per UE and report: DL/UL MCS, DL/UL BLER, PUSCH/PUCCH SNR, added to the
 * current 1 s window (output/csv.c divides by the number of reports).
 */
#ifndef E2E_CTRL_COLLECT_MAC_H
#define E2E_CTRL_COLLECT_MAC_H

#include <stddef.h>

#include "xApp/e42_xapp_api.h"

/* di = DU index (0 = DU1). Called from the E2 callback thread. */
void collect_mac(size_t di, sm_ag_if_rd_t const* rd);

#endif
