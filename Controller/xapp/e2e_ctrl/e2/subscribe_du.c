/* e2/subscribe_du.c - MAC/RLC/KPM subscriptions per DU. See subscribe_du.h. */
#include "e2/subscribe_du.h"

#include <stdio.h>

#include "collect/kpm_du.h"
#include "collect/mac.h"
#include "collect/rlc.h"
#include "config.h"
#include "e2/kpm_sub.h"
#include "e2/ran_func.h"

/* FlexRIC's callback type (sm_cb) has no user pointer, so one tiny callback per DU slot
 * passes the DU index on to the collect/ modules. */
#define DU_CALLBACKS(i)                                                        \
  static void mac_cb_##i(sm_ag_if_rd_t const* rd) { collect_mac(i, rd); }     \
  static void rlc_cb_##i(sm_ag_if_rd_t const* rd) { collect_rlc(i, rd); }     \
  static void kpm_cb_##i(sm_ag_if_rd_t const* rd) { collect_kpm_du(i, rd); }
DU_CALLBACKS(0)
DU_CALLBACKS(1)
DU_CALLBACKS(2)
DU_CALLBACKS(3)
static sm_cb const MAC_CB[MAX_DU] = {mac_cb_0, mac_cb_1, mac_cb_2, mac_cb_3};
static sm_cb const RLC_CB[MAX_DU] = {rlc_cb_0, rlc_cb_1, rlc_cb_2, rlc_cb_3};
static sm_cb const KPM_CB[MAX_DU] = {kpm_cb_0, kpm_cb_1, kpm_cb_2, kpm_cb_3};

/* MAC or RLC: the "data" argument is just the report interval string */
static sm_ans_xapp_t subscribe_interval(size_t di, du_info_t const* du, uint32_t rf, const char* name,
                                        const char* interval, sm_cb cb)
{
  sm_ans_xapp_t h = {0};
  if (ran_func_index(du->node, rf) == du->node->len_rf) {
    printf("[e2e_ctrl] DU%zu nb_id %u: no %s RAN function\n", di + 1, du->nb_id, name);
    return h;
  }
  h = report_sm_xapp_api(&du->node->id, rf, (void*)interval, cb);
  printf("[e2e_ctrl] DU%zu nb_id %u: %s %s every %s\n", di + 1, du->nb_id, name, h.success ? "subscribed" : "FAILED", interval);
  return h;
}

void subscribe_du(size_t di, du_info_t const* du, du_subs_t* subs)
{
  char label[32];
  snprintf(label, sizeof(label), "DU%zu nb_id %u", di + 1, du->nb_id);
  subs->mac = subscribe_interval(di, du, RF_MAC, "MAC", MAC_INTERVAL, MAC_CB[di]);
  subs->rlc = subscribe_interval(di, du, RF_RLC, "RLC", RLC_INTERVAL, RLC_CB[di]);
  subs->kpm = kpm_subscribe_style_4(du->node, KPM_CB[di], label);
}

void unsubscribe_du(du_subs_t const* subs)
{
  if (subs->mac.success)
    rm_report_sm_xapp_api(subs->mac.u.handle);
  if (subs->rlc.success)
    rm_report_sm_xapp_api(subs->rlc.u.handle);
  if (subs->kpm.success)
    rm_report_sm_xapp_api(subs->kpm.u.handle);
}
