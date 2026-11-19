/* e2/kpm_sub.c - KPM style 4 subscription data. See kpm_sub.h. */
#include "e2/kpm_sub.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "e2/ran_func.h"

/* Matching condition: S-NSSAI equal to the lab slice (1 byte SST + 3 bytes SD) */
static test_info_lst_t snssai_filter(void)
{
  static const uint8_t sd[3] = SNSSAI_SD_INIT;
  test_info_lst_t t = {0};
  t.test_cond_type = S_NSSAI_TEST_COND_TYPE;
  t.S_NSSAI = TRUE_TEST_COND_TYPE;

  t.test_cond = calloc(1, sizeof(test_cond_e));
  assert(t.test_cond != NULL);
  *t.test_cond = EQUAL_TEST_COND;

  t.test_cond_value = calloc(1, sizeof(test_cond_value_t));
  assert(t.test_cond_value != NULL);
  t.test_cond_value->type = OCTET_STRING_TEST_COND_VALUE;
  t.test_cond_value->octet_string_value = calloc(1, sizeof(byte_array_t));
  assert(t.test_cond_value->octet_string_value != NULL);
  byte_array_t* ba = t.test_cond_value->octet_string_value;
  ba->len = 4;
  ba->buf = calloc(ba->len, sizeof(uint8_t));
  assert(ba->buf != NULL);
  ba->buf[0] = SNSSAI_SST;
  memcpy(ba->buf + 1, sd, 3);
  return t;
}

/* Action definition format 1 inside format 4: the measurement list */
static void fill_meas_list(kpm_act_def_format_1_t* f1, ric_report_style_item_t const* item)
{
  size_t const sz = item->meas_info_for_action_lst_len;
  f1->meas_info_lst_len = sz;
  f1->meas_info_lst = calloc(sz, sizeof(meas_info_format_1_lst_t));
  assert(f1->meas_info_lst != NULL);
  for (size_t i = 0; i < sz; i++) {
    meas_info_format_1_lst_t* mi = &f1->meas_info_lst[i];
    mi->meas_type.type = NAME_MEAS_TYPE;
    mi->meas_type.name = copy_byte_array(item->meas_info_for_action_lst[i].name);
    mi->label_info_lst_len = 1;
    mi->label_info_lst = calloc(1, sizeof(label_info_lst_t));
    assert(mi->label_info_lst != NULL);
    mi->label_info_lst[0].noLabel = calloc(1, sizeof(enum_value_e));
    assert(mi->label_info_lst[0].noLabel != NULL);
    *mi->label_info_lst[0].noLabel = TRUE_ENUM_VALUE;
  }
  f1->gran_period_ms = KPM_PERIOD_MS;
  f1->cell_global_id = NULL;
#if defined KPM_V2_03 || defined KPM_V3_00
  f1->meas_bin_range_info_lst_len = 0;
  f1->meas_bin_info_lst = NULL;
#endif
}

kpm_sub_data_t kpm_sub_style_4(ric_report_style_item_t const* item)
{
  kpm_sub_data_t sub = {0};
  sub.ev_trg_def.type = FORMAT_1_RIC_EVENT_TRIGGER;
  sub.ev_trg_def.kpm_ric_event_trigger_format_1.report_period_ms = KPM_PERIOD_MS;

  sub.sz_ad = 1;
  sub.ad = calloc(1, sizeof(kpm_act_def_t));
  assert(sub.ad != NULL);
  kpm_act_def_t* ad = sub.ad;
  ad->type = FORMAT_4_ACTION_DEFINITION;

  ad->frm_4.matching_cond_lst_len = 1;
  ad->frm_4.matching_cond_lst = calloc(1, sizeof(matching_condition_format_4_lst_t));
  assert(ad->frm_4.matching_cond_lst != NULL);
  ad->frm_4.matching_cond_lst[0].test_info_lst = snssai_filter();

  fill_meas_list(&ad->frm_4.action_def_format_1, item);
  return sub;
}

/* Prints the subscribed measurement names, so the log shows what the node offers */
static void print_meas(const char* label, ric_report_style_item_t const* item)
{
  for (size_t i = 0; i < item->meas_info_for_action_lst_len; i++) {
    byte_array_t const name = item->meas_info_for_action_lst[i].name;
    printf("[e2e_ctrl] %s KPM measurement: %.*s\n", label, (int)name.len, (char const*)name.buf);
  }
}

sm_ans_xapp_t kpm_subscribe_style_4(e2_node_connected_xapp_t* n, sm_cb cb, const char* label)
{
  sm_ans_xapp_t h = {0};
  size_t const k = ran_func_index(n, RF_KPM);
  if (k == n->len_rf || n->rf[k].defn.type != KPM_RAN_FUNC_DEF_E) {
    printf("[e2e_ctrl] %s: no KPM RAN function\n", label);
    return h;
  }
  kpm_ran_function_def_t const* def = &n->rf[k].defn.kpm;
  for (size_t j = 0; j < def->sz_ric_report_style_list; j++) {
    ric_report_style_item_t const* item = &def->ric_report_style_list[j];
    if (item->report_style_type != STYLE_4_RIC_SERVICE_REPORT || item->act_def_format_type != FORMAT_4_ACTION_DEFINITION)
      continue;
    kpm_sub_data_t sub = kpm_sub_style_4(item);
    h = report_sm_xapp_api(&n->id, RF_KPM, &sub, cb);
    free_kpm_sub_data(&sub);
    printf("[e2e_ctrl] %s: KPM style 4 %s every %u ms (%zu measurements)\n", label, h.success ? "subscribed" : "FAILED",
           KPM_PERIOD_MS, item->meas_info_for_action_lst_len);
    print_meas(label, item);
    return h;
  }
  printf("[e2e_ctrl] %s: KPM has no REPORT style 4\n", label);
  return h;
}
