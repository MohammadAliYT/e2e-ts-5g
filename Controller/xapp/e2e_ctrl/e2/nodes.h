/* e2/nodes.h - find the DUs among the connected E2 nodes.
 *
 * Lists every E2 node the RIC knows, keeps the gNB-DUs (stage 1: no subscription on
 * the CU), numbers them by ascending nb_id (DU1 = 3584, DU2 = 3585 in the lab) and
 * reads each DU's NR Cell ID from its F1 Setup Request.
 */
#ifndef E2E_CTRL_NODES_H
#define E2E_CTRL_NODES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "xApp/e42_xapp_api.h"

typedef struct {
  e2_node_connected_xapp_t* node; /* points into the node list, valid until free_e2_node_arr_xapp() */
  uint32_t nb_id;
  bool has_cell;
  uint64_t nr_cell_id;
} du_info_t;

/* Fills dus[0..n-1] (DU1 first) from the node list and returns n (at most MAX_DU). */
size_t nodes_find_dus(e2_node_arr_xapp_t* nodes, du_info_t dus[MAX_DU]);

#endif
