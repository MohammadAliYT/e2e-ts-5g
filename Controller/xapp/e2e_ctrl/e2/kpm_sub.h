/* e2/kpm_sub.h - build a KPM REPORT style 4 subscription (per-UE measurements).
 *
 * Style 4 = UE-level reports (indication format 3), filtered to the lab slice
 * (S-NSSAI in config.h), one "noLabel" per measurement, every KPM_PERIOD_MS.
 * Same as fill_report_style_4() in examples/xApp/c/monitor/xapp_kpm_moni.c.
 */
#ifndef E2E_CTRL_KPM_SUB_H
#define E2E_CTRL_KPM_SUB_H

#include "xApp/e42_xapp_api.h"

/* Subscription data for every measurement the node lists for this report style.
 * Free the result with free_kpm_sub_data(). */
kpm_sub_data_t kpm_sub_style_4(ric_report_style_item_t const* item);

/* Subscribes node n's KPM REPORT style 4 (all its measurements) with callback cb.
 * label is only for the log line ("DU1", "CU"). Returns .success false if the node
 * has no KPM or no style 4. */
sm_ans_xapp_t kpm_subscribe_style_4(e2_node_connected_xapp_t* n, sm_cb cb, const char* label);

#endif
