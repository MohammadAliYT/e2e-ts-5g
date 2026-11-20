/* e2/subscribe_cu.c - KPM + RC subscriptions on the CU, with the GTP/PDCP guard. See subscribe_cu.h. */
#include "e2/subscribe_cu.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "collect/kpm_cu.h"
#include "collect/rc_meas.h"
#include "config.h"
#include "e2/kpm_sub.h"
#include "e2/rc_meas_sub.h"

/* Every CU subscription goes through here first. Subscribing GTP or PDCP on the CU is a
 * programming error: stop the controller before the CU gets hurt. */
static void cu_guard(uint32_t rf)
{
  if (rf == RF_GTP || rf == RF_PDCP) {
    fprintf(stderr, "[e2e_ctrl] REFUSED: service model %u must never be subscribed on the CU (see config.h)\n", rf);
    exit(EXIT_FAILURE);
  }
}

void subscribe_cu(cu_info_t const* cu, cu_subs_t* subs)
{
  char label[32];
  snprintf(label, sizeof(label), "CU nb_id %u", cu->nb_id);
  cu_guard(RF_KPM);
  subs->kpm = kpm_subscribe_style_4(cu->node, collect_kpm_cu, label);
  cu_guard(RF_RC);
  subs->rc_meas = rc_meas_subscribe(cu->node, collect_rc_meas, label);
}

void unsubscribe_cu(cu_subs_t const* subs)
{
  if (subs->rc_meas.success)
    rm_report_sm_xapp_api(subs->rc_meas.u.handle);
  if (subs->kpm.success)
    rm_report_sm_xapp_api(subs->kpm.u.handle);
}
