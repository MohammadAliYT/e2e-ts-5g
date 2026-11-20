/* loop/tick.h - the 1 s control loop of the controller.
 *
 * At every full wall-clock second the loop takes the lock and:
 *   1. closes the window of every DU entry (main CSV row, cell load, UE view),
 *   2. closes the CU window,
 *   3. updates the UE view (serving DU, handovers, activity, medians),
 *   4. writes the state CSV.
 *   5. confirms or times out pending handovers (act/act.h),
 *   6. releases the lock and runs the FIFO commands (handover CONTROL, "show").
 * Phase 4 adds "decide" between steps 3 and 6.
 */
#ifndef E2E_CTRL_TICK_H
#define E2E_CTRL_TICK_H

#include "e2/nodes.h"

/* Starts the loop thread. ran must stay valid until tick_stop(). */
void tick_start(ran_nodes_t const* ran);

/* Stops the loop thread (waits for the current window to finish). */
void tick_stop(void);

#endif
