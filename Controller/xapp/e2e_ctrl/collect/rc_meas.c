/* collect/rc_meas.c - RC message copies (MeasurementReports) -> CU table. See rc_meas.h. */
#include "collect/rc_meas.h"

#include <stdio.h>

#include "collect/cu_ue_id.h"
#include "collect/meas_report_dec.h"
#include "config.h"
#include "lib/sm/dec/dec_ue_id.h"
#include "state/cu_table.h"
#include "state/ue_table.h"
#include "util/time_now_us.h"

/* Octet string of RAN parameter id in a format 1 indication message, or an empty byte array */
static byte_array_t octets(e2sm_rc_ind_msg_frmt_1_t const* m, uint32_t id)
{
  for (size_t i = 0; i < m->sz_seq_ran_param; i++) {
    seq_ran_param_t const* p = &m->seq_ran_param[i];
    if (p->ran_param_id != id)
      continue;
    ran_parameter_value_t const* v = NULL;
    if (p->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE)
      v = p->ran_param_val.flag_false;
    else if (p->ran_param_val.type == ELEMENT_KEY_FLAG_TRUE_RAN_PARAMETER_VAL_TYPE)
      v = p->ran_param_val.flag_true;
    if (v != NULL && v->type == OCTET_STRING_RAN_PARAMETER_VALUE)
      return v->octet_str_ran;
  }
  return (byte_array_t){0};
}

/* CU UE ID from the APER-encoded UEID (same decoding as xapp_rc_moni.c) */
static bool decode_cu_ue_id(byte_array_t ba, uint32_t* cu_id)
{
  UEID_t asn = {0};
  UEID_t* ref = &asn;
  asn_dec_rval_t const rv = aper_decode(NULL, &asn_DEF_UEID, (void**)&ref, ba.buf, ba.len, 0, 0);
  bool ok = false;
  if (rv.code == RC_OK) {
    ue_id_e2sm_t id = dec_ue_id_asn(&asn);
    ok = id.type == GNB_UE_ID_E2SM && cu_ue_id_of(&id.gnb, cu_id);
    free_ue_id_e2sm(&id);
  }
  ASN_STRUCT_RESET(asn_DEF_UEID, &asn);
  return ok;
}

static void print_report(uint32_t cu_id, meas_report_t const* r)
{
  printf("[e2e_ctrl] RC meas: CU UE ID %u first report: serving PCI %u %d dBm", cu_id, r->srv.pci, r->srv.rsrp_dbm);
  for (size_t i = 0; i < r->n_nb; i++)
    printf(", neighbour PCI %u %d dBm", r->nb[i].pci, r->nb[i].rsrp_dbm);
  printf(r->n_nb == 0 ? ", no neighbour\n" : "\n");
}

void collect_rc_meas(sm_ag_if_rd_t const* rd)
{
  static uint32_t n_no_ue_id, n_bad;
  if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0 || rd->ind.type != RAN_CTRL_STATS_V1_03)
    return;
  rc_ind_data_t const* ind = &rd->ind.rc.ind;
  if (ind->msg.format != FORMAT_1_E2SM_RC_IND_MSG)
    return;

  byte_array_t const rrc = octets(&ind->msg.frmt_1, E2SM_RC_RS1_RRC_MESSAGE);
  byte_array_t const ueid = octets(&ind->msg.frmt_1, E2SM_RC_RS1_UE_ID);
  uint32_t cu_id;
  if (ueid.buf == NULL || !decode_cu_ue_id(ueid, &cu_id)) {
    if (n_no_ue_id++ % 100 == 0)
      fprintf(stderr, "[e2e_ctrl] RC meas: copy without a usable UE ID (%u so far) - CU without oai-cu-rc-msg-copy-ueid.patch?\n",
              n_no_ue_id);
    return;
  }
  meas_report_t r;
  if (!meas_report_decode(rrc.buf, rrc.len, &r) || !r.has_srv) {
    if (n_bad++ % 100 == 0)
      fprintf(stderr, "[e2e_ctrl] RC meas: CU UE ID %u: copy is no MeasurementReport with a serving cell (%u so far)\n", cu_id,
              n_bad);
    return;
  }

  int64_t const now = time_now_us();
  ue_table_lock();
  cu_ue_t* u = cu_table_get(cu_id);
  if (u != NULL) {
    if (u->meas.n_reports == 0)
      print_report(cu_id, &r);
    u->last_seen_us = now;
    cu_meas_update(&u->meas, &r, now);
    u->w_n_meas++;
  }
  ue_table_unlock();
}
