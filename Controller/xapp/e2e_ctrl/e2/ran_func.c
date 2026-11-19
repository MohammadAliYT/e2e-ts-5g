/* e2/ran_func.c - RAN function lookup. See ran_func.h. */
#include "e2/ran_func.h"

size_t ran_func_index(e2_node_connected_xapp_t const* n, uint32_t id)
{
  for (size_t i = 0; i < n->len_rf; i++)
    if (n->rf[i].id == id)
      return i;
  return n->len_rf;
}
