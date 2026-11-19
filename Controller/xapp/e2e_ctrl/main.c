/* main.c - e2e_ctrl, the E2E traffic steering controller (FlexRIC xApp).
 *
 * Phase 0/stage 1 "Observe": find the DUs, subscribe MAC + RLC + KPM on each,
 * write one CSV row per UE per second. Nothing is sent back to the RAN yet.
 *
 *   e2/       talk to the RAN over E2 (node list, F1 Setup, subscriptions)
 *   collect/  turn E2 reports into UE state (one file per service model)
 *   state/    the UE table
 *   output/   CSV writer
 *   config.h  all fixed settings
 *
 * Run inside netns e2e-ran; stops on Ctrl+C / SIGTERM or after XAPP_DURATION seconds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "e2/nodes.h"
#include "e2/subscribe.h"
#include "output/csv.h"
#include "state/ue_table.h"
#include "xApp/e42_xapp_api.h"

int main(int argc, char* argv[])
{
  fr_args_t args = init_fr_args(argc, argv);
  csv_open();

  init_xapp_api(&args);
  sleep(1);

  /* 1. which DUs are there */
  e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
  du_info_t dus[MAX_DU] = {0};
  size_t const n_du = nodes_find_dus(&nodes, dus);
  if (n_du == 0)
    fprintf(stderr, "[e2e_ctrl] no DU connected - nothing to observe\n");

  /* 2. UE table before the first report can arrive */
  uint32_t nb_id[MAX_DU] = {0};
  for (size_t di = 0; di < n_du; di++)
    nb_id[di] = dus[di].nb_id;
  ue_table_init(n_du, nb_id);

  /* 3. subscribe, 4. write the CSV every second until stopped */
  du_subs_t subs[MAX_DU] = {0};
  for (size_t di = 0; di < n_du; di++)
    subscribe_du(di, &dus[di], &subs[di]);
  csv_start(n_du, dus);

  xapp_wait_end_api(); /* returns on Ctrl+C / SIGTERM or after XAPP_DURATION seconds */

  /* 5. shut down in reverse order */
  csv_stop();
  for (size_t di = 0; di < n_du; di++)
    unsubscribe_du(&subs[di]);
  free_e2_node_arr_xapp(&nodes);

  while (try_stop_xapp_api() == false)
    usleep(1000);

  printf("[e2e_ctrl] stopped\n");
  return EXIT_SUCCESS;
}
