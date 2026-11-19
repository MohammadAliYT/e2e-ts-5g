/* e2/ran_func.h - look up a RAN function (service model) in an E2 node's list. */
#ifndef E2E_CTRL_RAN_FUNC_H
#define E2E_CTRL_RAN_FUNC_H

#include <stddef.h>
#include <stdint.h>

#include "xApp/e42_xapp_api.h"

/* Index of RAN function id in n->rf, or n->len_rf if the node does not offer it. */
size_t ran_func_index(e2_node_connected_xapp_t const* n, uint32_t id);

#endif
