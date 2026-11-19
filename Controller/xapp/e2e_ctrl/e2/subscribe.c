/* e2/subscribe.c - MAC/RLC/KPM subscriptions per DU. See subscribe.h. */
#include "e2/subscribe.h"

#include <stdio.h>

#include "collect/kpm_du.h"
#include "collect/mac.h"
#include "collect/rlc.h"
#include "config.h"
#include "e2/kpm_sub.h"

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

/* Index of RAN function id in the node's list, or len_rf if missing */
static size_t find_rf(e2_node_connected_xapp_t const* n, uint32_t id)
{
  for (size_t i = 0; i < n->len_rf; i++)
    if (n->rf[i].id == id)
      return i;
  return n->len_rf;
}

/* MAC or RLC: the "data" argument is just the report interval string */
static sm_ans_xapp_t subscribe_interval(size_t di, du_info_t const* du, uint32_t rf, const char* name,
                                        const char* interval, sm_cb cb)
{
  sm_ans_xapp_t h = {0};
  if (find_rf(du->node, rf) == du->node->len_rf) {
    printf("[e2e_ctrl] DU%zu nb_id %u: no %s RAN function\n", di + 1, du->nb_id, name);
    return h;
  }
  h = report_sm_xapp_api(&du->node->id, rf, (void*)interval, cb);
  printf("[e2e_ctrl] DU%zu nb_id %u: %s %s every %s\n", di + 1, du->nb_id, name, h.success ? "subscribed" : "FAILED", interval);
  return h;
}

/* KPM: use the node's REPORT style 4 with all its measurements */
static sm_ans_xapp_t subscribe_kpm(size_t di, du_info_t const* du)
{
  sm_ans_xapp_t h = {0};
  e2_node_connected_xapp_t* n = du->node;
  size_t const k = find_rf(n, RF_KPM);
  if (k == n->len_rf || n->rf[k].defn.type != KPM_RAN_FUNC_DEF_E) {
    printf("[e2e_ctrl] DU%zu nb_id %u: no KPM RAN function\n", di + 1, du->nb_id);
    return h;
  }
  kpm_ran_function_def_t const* def = &n->rf[k].defn.kpm;
  for (size_t j = 0; j < def->sz_ric_report_style_list; j++) {
    ric_report_style_item_t const* item = &def->ric_report_style_list[j];
    if (item->report_style_type != STYLE_4_RIC_SERVICE_REPORT || item->act_def_format_type != FORMAT_4_ACTION_DEFINITION)
      continue;
    kpm_sub_data_t sub = kpm_sub_style_4(item);
    h = report_sm_xapp_api(&n->id, RF_KPM, &sub, KPM_CB[di]);
    free_kpm_sub_data(&sub);
    printf("[e2e_ctrl] DU%zu nb_id %u: KPM style 4 %s every %u ms (%zu measurements)\n", di + 1, du->nb_id,
           h.success ? "subscribed" : "FAILED", KPM_PERIOD_MS, item->meas_info_for_action_lst_len);
    return h;
  }
  printf("[e2e_ctrl] DU%zu nb_id %u: KPM has no REPORT style 4\n", di + 1, du->nb_id);
  return h;
}

void subscribe_du(size_t di, du_info_t const* du, du_subs_t* subs)
{
  subs->mac = subscribe_interval(di, du, RF_MAC, "MAC", MAC_INTERVAL, MAC_CB[di]);
  subs->rlc = subscribe_interval(di, du, RF_RLC, "RLC", RLC_INTERVAL, RLC_CB[di]);
  subs->kpm = subscribe_kpm(di, du);
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
