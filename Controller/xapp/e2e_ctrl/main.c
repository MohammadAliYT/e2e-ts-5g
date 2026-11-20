/* main.c - e2e_ctrl, the E2E traffic steering controller (FlexRIC xApp).
 *
 * Phase 3 "Act on command": find the CU and the DUs; subscribe MAC + RLC + KPM on each DU
 * and KPM on the CU (never GTP/PDCP on the CU). Every second: write one CSV row per DU entry,
 * update the UE view (per CU UE ID: serving DU, RNTI, handovers, activity, medians), write
 * one state CSV row per UE, and run the commands from the FIFO (act/cmd_fifo.h):
 * "ho <CU UE ID> <DU number>" sends an RC handover with that DU's cell as explicit target.
 *
 *   e2/       talk to the RAN over E2 (node list, F1 Setup, subscriptions)
 *   collect/  turn E2 reports into state (one file per report type)
 *   state/    DU tables (per RNTI), CU table, UE view (per CU UE ID), cell view, medians
 *   act/      commands (FIFO), handover requests: check, send, confirm
 *   loop/     the 1 s control loop
 *   output/   CSV files
 *   config.h  all fixed settings
 *
 * Run inside netns e2e-ran; stops on Ctrl+C / SIGTERM or after XAPP_DURATION seconds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "act/act.h"
#include "config.h"
#include "e2/nodes.h"
#include "e2/subscribe_cu.h"
#include "e2/subscribe_du.h"
#include "loop/tick.h"
#include "output/csv.h"
#include "output/state_csv.h"
#include "state/ue_table.h"
#include "xApp/e42_xapp_api.h"

int main(int argc, char* argv[])
{
  fr_args_t args = init_fr_args(argc, argv);
  csv_open();
  state_csv_open();

  init_xapp_api(&args);
  sleep(1);

  /* 1. which CU and DUs are there */
  e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
  ran_nodes_t ran;
  nodes_find(&nodes, &ran);
  if (ran.n_du == 0)
    fprintf(stderr, "[e2e_ctrl] no DU connected - nothing to observe\n");

  /* 2. UE table before the first report can arrive */
  uint32_t nb_id[MAX_DU] = {0};
  for (size_t di = 0; di < ran.n_du; di++)
    nb_id[di] = ran.du[di].nb_id;
  ue_table_init(ran.n_du, nb_id);

  /* 3. subscribe, 4. run the 1 s loop until stopped */
  du_subs_t du_subs[MAX_DU] = {0};
  for (size_t di = 0; di < ran.n_du; di++)
    subscribe_du(di, &ran.du[di], &du_subs[di]);
  cu_subs_t cu_subs = {0};
  if (ran.has_cu)
    subscribe_cu(&ran.cu, &cu_subs);
  act_init(&ran);
  tick_start(&ran);

  xapp_wait_end_api(); /* returns on Ctrl+C / SIGTERM or after XAPP_DURATION seconds */

  /* 5. shut down in reverse order */
  tick_stop();
  act_stop();
  state_csv_close();
  csv_close();
  unsubscribe_cu(&cu_subs);
  for (size_t di = 0; di < ran.n_du; di++)
    unsubscribe_du(&du_subs[di]);
  free_e2_node_arr_xapp(&nodes);

  while (try_stop_xapp_api() == false)
    usleep(1000);

  printf("[e2e_ctrl] stopped\n");
  return EXIT_SUCCESS;
}
