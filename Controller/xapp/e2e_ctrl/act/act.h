/* act/act.h - phase 3 "act on command": RAN handover requested through the FIFO.
 *
 * Per handover request:
 *   1. check (lock held): UE known and reporting, target DU exists and has a cell,
 *      target is not the serving DU, no request for this UE is pending,
 *      the last request for this UE is at least HO_COOLDOWN_S old
 *   2. send (lock NOT held): RC CONTROL Handover Control to the CU (e2/rc_handover.h)
 *   3. confirm (lock held, every window): the UE view shows the target DU as serving DU
 *      -> "confirmed after x s"; no change after HO_CONFIRM_S -> "NOT confirmed"
 * Every step prints one "[act]" line on stderr.
 * Phase 4 (decide) uses act_request() too, with the same checks.
 */
#ifndef E2E_CTRL_ACT_H
#define E2E_CTRL_ACT_H

#include <stdbool.h>
#include <stdint.h>

#include "e2/nodes.h"

/* Call after the subscriptions. Checks RC handover support on the CU and starts the FIFO. */
void act_init(ran_nodes_t const* ran);
void act_stop(void);

/* Ask for a handover of CU UE ID id to DU index to_di (0 = DU1). Lock NOT held.
 * Returns true if the CONTROL was sent. */
bool act_request(uint32_t id, int to_di, int64_t t_rel);

/* Take the FIFO commands and run them. Call once per window, lock NOT held. */
void act_run_commands(int64_t t_rel);

/* Confirm or time out pending handovers. Call once per window after
 * ue_view_end_window(), lock held. */
void act_check_pending(int64_t t_rel, int64_t now_us);

#endif
