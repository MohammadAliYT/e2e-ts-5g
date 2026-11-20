/* e2/rc_handover.h - E2SM-RC CONTROL style 3 (connected mode mobility), action 1
 * (Handover Control), sent to the CU.
 *
 * The CU (OAI ran_func_rc.c, write_ctrl_conn_mode_mobility) finds the UE by the RAN UE ID
 * in a gNB-type UE ID (= CU UE ID) and the target cell by the NR Cell Identity in the
 * NR CGI. If the target cell is one of its DUs, it starts an F1 handover.
 * Message layout from FlexRIC examples/xApp/c/rc_handover/xapp_rc_handover.c
 * (gen_handover_ctrl, fill_target_primary_cell_id).
 */
#ifndef E2E_CTRL_RC_HANDOVER_H
#define E2E_CTRL_RC_HANDOVER_H

#include <stdbool.h>
#include <stdint.h>

#include "e2/f1_setup.h"
#include "e2/nodes.h"
#include "lib/sm/ie/ue_id.h"

/* True if the CU offers RC CONTROL style 3 with Handover Control. Prints one line. */
bool rc_handover_supported(cu_info_t const* cu);

/* Send the handover CONTROL and wait for the CU's answer.
 * BLOCKS up to ~6 s, and FlexRIC aborts the program if no answer arrives
 * (cond_wait_sync_ui assert). Never call it while holding ue_table_lock():
 * the E2 report callbacks need that lock to run. */
void rc_handover_send(cu_info_t const* cu, ue_id_e2sm_t const* ue_id, uint8_t const nr_cgi[NR_CGI_LEN]);

#endif
