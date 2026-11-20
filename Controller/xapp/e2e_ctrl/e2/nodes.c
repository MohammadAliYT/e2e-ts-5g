/* e2/nodes.c - E2 node list -> CU + sorted DU list with cell IDs. See nodes.h. */
#include "e2/nodes.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "e2/f1_setup.h"

static const char* node_type_name(ngran_node_t t)
{
  switch (t) {
    case ngran_gNB: return "gNB";
    case ngran_gNB_CU: return "gNB-CU";
    case ngran_gNB_DU: return "gNB-DU";
    case ngran_gNB_CUCP: return "gNB-CU-CP";
    case ngran_gNB_CUUP: return "gNB-CU-UP";
    default: return "other";
  }
}

static int cmp_nb_id(const void* a, const void* b)
{
  uint32_t const x = ((du_info_t const*)a)->nb_id;
  uint32_t const y = ((du_info_t const*)b)->nb_id;
  return (x > y) - (x < y);
}

/* NR Cell ID from the F1 component of the node's E2 Node Component Configuration */
static void read_cell_id(du_info_t* d)
{
  e2_node_connected_xapp_t const* n = d->node;
  for (size_t j = 0; j < n->len_cca && !d->has_cell; j++)
    if (n->cca[j].e2_node_comp_interface_type == F1_E2AP_NODE_COMP_INTERFACE_TYPE)
      d->has_cell = f1_setup_cell(n->cca[j].e2_node_comp_conf.request, &d->nr_cell_id, d->nr_cgi, &d->pci);
}

/* Sort one node into ran: DU, CU, or ignored */
static void add_node(ran_nodes_t* ran, e2_node_connected_xapp_t* n)
{
  uint32_t const nb_id = n->id.nb_id.nb_id;
  if (n->id.type == ngran_gNB_DU) {
    if (ran->n_du == MAX_DU) {
      printf("[e2e_ctrl]   more than %d DUs, ignored\n", MAX_DU);
      return;
    }
    ran->du[ran->n_du++] = (du_info_t){.node = n, .nb_id = nb_id};
  } else if (n->id.type == ngran_gNB_CU) {
    if (ran->has_cu) {
      printf("[e2e_ctrl]   second CU, ignored\n");
      return;
    }
    ran->has_cu = true;
    ran->cu = (cu_info_t){.node = n, .nb_id = nb_id};
  } else {
    printf("[e2e_ctrl]   neither gNB-CU nor gNB-DU, ignored\n");
  }
}

static void count_nodes(e2_node_arr_xapp_t const* nodes, size_t* n_du, size_t* n_cu)
{
  *n_du = *n_cu = 0;
  for (size_t i = 0; i < nodes->len; i++) {
    if (nodes->n[i].id.type == ngran_gNB_DU)
      (*n_du)++;
    else if (nodes->n[i].id.type == ngran_gNB_CU)
      (*n_cu)++;
  }
}

e2_node_arr_xapp_t nodes_wait(size_t want_du, int timeout_s, bool* ok)
{
  for (int t = 0;; t++) {
    e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
    size_t n_du, n_cu;
    count_nodes(&nodes, &n_du, &n_cu);
    *ok = n_du >= want_du && n_cu >= 1;
    if (*ok || t >= timeout_s) {
      if (!*ok)
        fprintf(stderr, "[e2e_ctrl] after %d s the RIC lists %zu DU(s) and %zu CU(s), expected %zu DU(s) and 1 CU\n",
                t, n_du, n_cu, want_du);
      return nodes;
    }
    if (t == 0)
      printf("[e2e_ctrl] waiting for %zu DU(s) and 1 CU at the RIC (now %zu DU(s), %zu CU(s)), at most %d s\n", want_du, n_du,
             n_cu, timeout_s);
    free_e2_node_arr_xapp(&nodes);
    sleep(1);
  }
}

void nodes_find(e2_node_arr_xapp_t* nodes, ran_nodes_t* ran)
{
  *ran = (ran_nodes_t){0};
  printf("[e2e_ctrl] connected E2 nodes: %u\n", nodes->len);
  for (size_t i = 0; i < nodes->len; i++) {
    e2_node_connected_xapp_t* n = &nodes->n[i];
    printf("[e2e_ctrl] node %zu: %s nb_id %u, %zu RAN functions\n", i, node_type_name(n->id.type), n->id.nb_id.nb_id, n->len_rf);
    add_node(ran, n);
  }

  /* DU numbering by ascending nb_id: DU1 = 3584 (0xe00), DU2 = 3585 (0xe01) in this lab */
  qsort(ran->du, ran->n_du, sizeof(ran->du[0]), cmp_nb_id);
  for (size_t di = 0; di < ran->n_du; di++) {
    du_info_t* d = &ran->du[di];
    read_cell_id(d);
    if (d->has_cell)
      printf("[e2e_ctrl] DU%zu = nb_id %u (0x%x), NR cell ID %" PRIu64 ", PCI %u\n", di + 1, d->nb_id, d->nb_id, d->nr_cell_id,
             d->pci);
    else
      printf("[e2e_ctrl] DU%zu = nb_id %u (0x%x), NR cell ID unknown (no F1 Setup in component config)\n", di + 1, d->nb_id, d->nb_id);
  }
  if (ran->has_cu)
    printf("[e2e_ctrl] CU  = nb_id %u (0x%x)\n", ran->cu.nb_id, ran->cu.nb_id);
  else
    printf("[e2e_ctrl] no CU connected\n");
}
