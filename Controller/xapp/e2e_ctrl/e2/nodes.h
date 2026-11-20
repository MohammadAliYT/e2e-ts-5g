/* e2/nodes.h - sort the connected E2 nodes into the CU and the DUs.
 *
 * Lists every E2 node the RIC knows. DUs are numbered by ascending nb_id
 * (DU1 = 3584, DU2 = 3585 in the lab) and get their NR Cell ID from the F1 Setup
 * Request. The CU (gNB-CU, one in the lab) is kept for its KPM reports now and for
 * RC handover control later.
 */
#ifndef E2E_CTRL_NODES_H
#define E2E_CTRL_NODES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "e2/f1_setup.h"
#include "xApp/e42_xapp_api.h"

typedef struct {
  e2_node_connected_xapp_t* node; /* points into the node list, valid until free_e2_node_arr_xapp() */
  uint32_t nb_id;
  bool has_cell;      /* nr_cell_id, nr_cgi and pci are valid */
  uint64_t nr_cell_id;
  uint16_t pci;       /* PCI of the DU's cell (MeasurementReports name cells by PCI) */
  uint8_t nr_cgi[NR_CGI_LEN]; /* target cell for an RC handover to this DU */
} du_info_t;

typedef struct {
  e2_node_connected_xapp_t* node; /* same lifetime as above */
  uint32_t nb_id;
} cu_info_t;

typedef struct {
  du_info_t du[MAX_DU]; /* du[0] = DU1 */
  size_t n_du;
  bool has_cu;
  cu_info_t cu;
} ran_nodes_t;

/* Start-up guard: reads the node list every second until it has want_du DUs and a CU, at most
 * timeout_s seconds. Returns the last list (free it with free_e2_node_arr_xapp()); *ok = false on timeout. */
e2_node_arr_xapp_t nodes_wait(size_t want_du, int timeout_s, bool* ok);

/* Fills ran from the node list and prints one line per node. */
void nodes_find(e2_node_arr_xapp_t* nodes, ran_nodes_t* ran);

#endif
