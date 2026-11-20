/* e2/rc_meas_sub.h - RC REPORT style 1 subscription on the CU: a copy of every UE MeasurementReport.
 *
 * Event trigger format 1, one item: RRC message, UL-DCCH, MeasurementReport (config.h).
 * Action definition format 1 with RAN parameters 3 "RRC Message" and 4 "UE ID": with parameter 4
 * the CU adds the UE ID to each copy (oai-cu-rc-msg-copy-ueid.patch, option b).
 * Same structures as gen_rc_sub_msg() / fill_msg_ev_trig_3() in examples/xApp/c/monitor/xapp_rc_moni.c.
 */
#ifndef E2E_CTRL_RC_MEAS_SUB_H
#define E2E_CTRL_RC_MEAS_SUB_H

#include "xApp/e42_xapp_api.h"

/* Subscribes node n with callback cb. label is only for the log line. Returns .success false if
 * the node offers no RC REPORT style 1 with both parameters. */
sm_ans_xapp_t rc_meas_subscribe(e2_node_connected_xapp_t* n, sm_cb cb, const char* label);

#endif
