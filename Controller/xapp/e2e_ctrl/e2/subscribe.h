/* e2/subscribe.h - E2 REPORT subscriptions per DU: MAC, RLC and KPM style 4.
 *
 * Each subscription delivers its reports to the matching collect/ module, tagged
 * with the DU index (0 = DU1), so the UE table knows which DU a report came from.
 */
#ifndef E2E_CTRL_SUBSCRIBE_H
#define E2E_CTRL_SUBSCRIBE_H

#include <stddef.h>

#include "e2/nodes.h"
#include "xApp/e42_xapp_api.h"

typedef struct {
  sm_ans_xapp_t mac, rlc, kpm; /* .success false = not subscribed */
} du_subs_t;

/* di = DU index (0..MAX_DU-1). Prints one line per subscription. */
void subscribe_du(size_t di, du_info_t const* du, du_subs_t* subs);

/* Removes every successful subscription in subs. */
void unsubscribe_du(du_subs_t const* subs);

#endif
