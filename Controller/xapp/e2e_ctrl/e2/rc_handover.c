/* e2/rc_handover.c - RC CONTROL Handover Control to the CU. See rc_handover.h. */
#include "e2/rc_handover.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "e2/ran_func.h"
#include "sm/rc_sm/ie/rc_data_ie.h"
#include "sm/rc_sm/ie/ir/ran_param_struct.h"

/* E2SM-RC 7.6.4: connected mode mobility control */
enum { RC_CTRL_STYLE_MOBILITY = 3 };

bool rc_handover_supported(cu_info_t const* cu)
{
  e2_node_connected_xapp_t const* n = cu->node;
  size_t const idx = ran_func_index(n, RF_RC);
  bool ok = false;
  if (idx < n->len_rf) {
    ran_func_def_ctrl_t const* ctrl = n->rf[idx].defn.rc.ctrl;
    for (size_t i = 0; ctrl != NULL && i < ctrl->sz_seq_ctrl_style && !ok; i++) {
      seq_ctrl_style_t const* st = &ctrl->seq_ctrl_style[i];
      if (st->style_type != RC_CTRL_STYLE_MOBILITY)
        continue;
      for (size_t j = 0; j < st->sz_seq_ctrl_act; j++)
        if (st->seq_ctrl_act[j].id == HANDOVER_CONTROL_7_6_4_1)
          ok = true;
    }
  }
  printf("[e2e_ctrl] CU nb_id %u: RC handover control %s\n", cu->nb_id, ok ? "available" : "NOT available");
  return ok;
}

/* One RAN parameter inside a one-member STRUCTURE */
static ran_param_val_type_t wrap_in_struct(seq_ran_param_t inner)
{
  ran_param_val_type_t v = {.type = STRUCTURE_RAN_PARAMETER_VAL_TYPE};
  v.strct = calloc(1, sizeof(ran_param_struct_t));
  assert(v.strct != NULL && "Memory exhausted");
  v.strct->sz_ran_param_struct = 1;
  v.strct->ran_param_struct = calloc(1, sizeof(seq_ran_param_t));
  assert(v.strct->ran_param_struct != NULL && "Memory exhausted");
  v.strct->ran_param_struct[0] = inner;
  return v;
}

/* Target Primary Cell ID > CHOICE Target Cell > NR Cell ID > NR CGI (E2SM-RC 8.4.4.1) */
static seq_ran_param_t target_cell(uint8_t const nr_cgi[NR_CGI_LEN])
{
  seq_ran_param_t cgi = {.ran_param_id = NR_CGI_8_4_4_1};
  cgi.ran_param_val.type = ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE;
  cgi.ran_param_val.flag_false = calloc(1, sizeof(ran_parameter_value_t));
  assert(cgi.ran_param_val.flag_false != NULL && "Memory exhausted");
  cgi.ran_param_val.flag_false->type = OCTET_STRING_RAN_PARAMETER_VALUE;
  byte_array_t ba = {.len = NR_CGI_LEN, .buf = malloc(NR_CGI_LEN)};
  assert(ba.buf != NULL && "Memory exhausted");
  memcpy(ba.buf, nr_cgi, NR_CGI_LEN);
  cgi.ran_param_val.flag_false->octet_str_ran = ba;

  seq_ran_param_t nr_cell = {.ran_param_id = NR_CELL_8_4_4_1};
  nr_cell.ran_param_val = wrap_in_struct(cgi);

  seq_ran_param_t choice = {.ran_param_id = CHOICE_TARGET_CELL_8_4_4_1};
  choice.ran_param_val = wrap_in_struct(nr_cell);

  seq_ran_param_t target = {.ran_param_id = TARGET_PRIMARY_CELL_ID_8_4_4_1};
  target.ran_param_val = wrap_in_struct(choice);
  return target;
}

void rc_handover_send(cu_info_t const* cu, ue_id_e2sm_t const* ue_id, uint8_t const nr_cgi[NR_CGI_LEN])
{
  rc_ctrl_req_data_t ctrl = {0};

  /* CONTROL header, E2SM-RC 9.2.2.11 */
  ctrl.hdr.format = FORMAT_1_E2SM_RC_CTRL_HDR;
  ctrl.hdr.frmt_1.ric_style_type = RC_CTRL_STYLE_MOBILITY;
  ctrl.hdr.frmt_1.ctrl_act_id = HANDOVER_CONTROL_7_6_4_1;
  ctrl.hdr.frmt_1.ue_id = cp_ue_id_e2sm(ue_id); /* freed by free_rc_ctrl_req_data */

  /* CONTROL message, E2SM-RC 9.2.2.12 */
  ctrl.msg.format = FORMAT_1_E2SM_RC_CTRL_MSG;
  ctrl.msg.frmt_1.sz_ran_param = 1;
  ctrl.msg.frmt_1.ran_param = calloc(1, sizeof(seq_ran_param_t));
  assert(ctrl.msg.frmt_1.ran_param != NULL && "Memory exhausted");
  ctrl.msg.frmt_1.ran_param[0] = target_cell(nr_cgi);

  control_sm_xapp_api(&cu->node->id, RF_RC, &ctrl); /* returns only after the CU's answer */
  free_rc_ctrl_req_data(&ctrl);
}
