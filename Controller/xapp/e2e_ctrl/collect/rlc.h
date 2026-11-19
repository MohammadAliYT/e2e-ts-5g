/* collect/rlc.h - RLC service model (ID 143) reports from a DU -> UE table.
 *
 * The DU reports cumulative byte counters per radio bearer. Throughput = change of
 * the counter / change of the DU timestamp:
 *   DL = txpdu_bytes on DRB 1, UL = rxpdu_bytes on DRB 2 (see config.h).
 * DL queue on DRB 1, every report: txsdu_wt_us (head-of-line delay = how long the oldest
 * waiting packet has been queued) and txbuf_occ_bytes (queued bytes).
 * Not txsdu_avg_time_to_tx: OAI resets that average on every RLC read (read_rlc_sm ->
 * active_avg_to_tx -> time_average_reset), so it is ~0 here and spoils KPM DRB.RlcSduDelayDl.
 */
#ifndef E2E_CTRL_COLLECT_RLC_H
#define E2E_CTRL_COLLECT_RLC_H

#include <stddef.h>

#include "xApp/e42_xapp_api.h"

/* di = DU index (0 = DU1). Called from the E2 callback thread. */
void collect_rlc(size_t di, sm_ag_if_rd_t const* rd);

#endif
