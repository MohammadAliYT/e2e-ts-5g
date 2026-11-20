/* e2/rc_meas_sub.c - RC REPORT style 1 (message copy) subscription. See rc_meas_sub.h. */
#include "e2/rc_meas_sub.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "e2/ran_func.h"
#include "sm/rc_sm/ie/rc_data_ie.h"

static bool has_param(seq_report_sty_t const* st, uint32_t id)
{
  for (size_t i = 0; i < st->sz_seq_ran_param; i++)
    if (st->ran_param[i].id == id)
      return true;
  return false;
}

/* REPORT style 1 with the RRC Message and UE ID parameters in the node's RAN function definition */
static bool offers_msg_copy(ran_func_def_report_t const* rep)
{
  for (size_t i = 0; rep != NULL && i < rep->sz_seq_report_sty; i++) {
    seq_report_sty_t const* st = &rep->seq_report_sty[i];
    if (st->report_type == RC_REPORT_STYLE_MSG_COPY && has_param(st, E2SM_RC_RS1_RRC_MESSAGE) && has_param(st, E2SM_RC_RS1_UE_ID))
      return true;
  }
  return false;
}

static rc_sub_data_t build_sub(void)
{
  rc_sub_data_t sub = {0};

  /* Event trigger format 1 (E2SM-RC 9.2.1.1.1): one message, UL-DCCH MeasurementReport */
  sub.et.format = FORMAT_1_E2SM_RC_EV_TRIGGER_FORMAT;
  sub.et.frmt_1.sz_msg_ev_trg = 1;
  sub.et.frmt_1.msg_ev_trg = calloc(1, sizeof(msg_ev_trg_t));
  assert(sub.et.frmt_1.msg_ev_trg != NULL && "Memory exhausted");
  msg_ev_trg_t* t = &sub.et.frmt_1.msg_ev_trg[0];
  t->ev_trigger_cond_id = RC_MEAS_COND_ID;
  t->msg_type = RRC_MSG_MSG_TYPE_EV_TRG;
  t->rrc_msg.type = NR_RRC_MESSAGE_ID;
  t->rrc_msg.nr = UL_DCCH_NR_RRC_CLASS;
  t->rrc_msg.rrc_msg_id = RRC_MSG_ID_MEAS_REPORT;
  /* msg_dir, assoc_ue_info, log_or: optional, NULL (free_msg_ev_trg asserts msg_dir == NULL) */

  /* Action definition format 1 (E2SM-RC 9.2.1.2.1): parameters to report */
  sub.sz_ad = 1;
  sub.ad = calloc(1, sizeof(e2sm_rc_action_def_t));
  assert(sub.ad != NULL && "Memory exhausted");
  sub.ad[0].ric_style_type = RC_REPORT_STYLE_MSG_COPY;
  sub.ad[0].format = FORMAT_1_E2SM_RC_ACT_DEF;
  sub.ad[0].frmt_1.sz_param_report_def = 2;
  sub.ad[0].frmt_1.param_report_def = calloc(2, sizeof(param_report_def_t));
  assert(sub.ad[0].frmt_1.param_report_def != NULL && "Memory exhausted");
  sub.ad[0].frmt_1.param_report_def[0].ran_param_id = E2SM_RC_RS1_RRC_MESSAGE;
  sub.ad[0].frmt_1.param_report_def[1].ran_param_id = E2SM_RC_RS1_UE_ID;
  return sub;
}

sm_ans_xapp_t rc_meas_subscribe(e2_node_connected_xapp_t* n, sm_cb cb, const char* label)
{
  sm_ans_xapp_t h = {0};
  size_t const k = ran_func_index(n, RF_RC);
  if (k == n->len_rf || n->rf[k].defn.type != RC_RAN_FUNC_DEF_E) {
    printf("[e2e_ctrl] %s: no RC RAN function\n", label);
    return h;
  }
  if (!offers_msg_copy(n->rf[k].defn.rc.report)) {
    printf("[e2e_ctrl] %s: RC has no REPORT style 1 with RRC Message + UE ID\n", label);
    return h;
  }
  rc_sub_data_t sub = build_sub();
  h = report_sm_xapp_api(&n->id, RF_RC, &sub, cb);
  free_rc_sub_data(&sub);
  printf("[e2e_ctrl] %s: RC style 1 (UL-DCCH MeasurementReport copy + UE ID) %s\n", label, h.success ? "subscribed" : "FAILED");
  return h;
}
