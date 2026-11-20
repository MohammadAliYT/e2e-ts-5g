/* e2/f1_setup.c - decode the F1 Setup Request -> NR Cell Identity + NR CGI. See f1_setup.h. */
#include "e2/f1_setup.h"

#include <stdio.h>
#include <string.h>

#include "F1AP_F1AP-PDU.h"
#include "F1AP_InitiatingMessage.h"
#include "F1AP_ProtocolIE-Field.h"
#include "aper_decoder.h"

/* NR Cell Identity: 36-bit BIT STRING, left-aligned in 5 octets (TS 38.473 9.3.1.7);
 * same layout as nr_cgi_cell_id() in examples/xApp/c/rc_handover/xapp_rc_handover.c */
static bool cell_id_from_bits(BIT_STRING_t const* cid, uint64_t* cell_id)
{
  if (cid->size != 5)
    return false;
  uint8_t const* b = cid->buf;
  *cell_id = ((uint64_t)b[0] << 28) | ((uint64_t)b[1] << 20) | ((uint64_t)b[2] << 12) | ((uint64_t)b[3] << 4) | (b[4] >> 4);
  return true;
}

/* Raw NR CGI: PLMN Identity (3 octets) + NR Cell Identity (5 octets, 4 unused bits) */
static bool nr_cgi_bytes(F1AP_NRCGI_t const* c, uint8_t out[NR_CGI_LEN])
{
  if (c->pLMN_Identity.size != 3 || c->nRCellIdentity.size != 5)
    return false;
  memcpy(out, c->pLMN_Identity.buf, 3);
  memcpy(out + 3, c->nRCellIdentity.buf, 5);
  return true;
}

/* First served cell in the GNB-DU Served Cells List IE, if any */
static bool served_cell(F1AP_F1SetupRequest_t const* r, uint64_t* cell_id, uint8_t nr_cgi[NR_CGI_LEN])
{
  for (int i = 0; i < r->protocolIEs.list.count; i++) {
    F1AP_F1SetupRequestIEs_t const* ie = r->protocolIEs.list.array[i];
    if (ie->id != F1AP_ProtocolIE_ID_id_gNB_DU_Served_Cells_List)
      continue;
    F1AP_GNB_DU_Served_Cells_List_t const* cells = &ie->value.choice.GNB_DU_Served_Cells_List;
    if (cells->list.count < 1)
      continue;
    F1AP_GNB_DU_Served_Cells_ItemIEs_t const* item = (F1AP_GNB_DU_Served_Cells_ItemIEs_t const*)cells->list.array[0];
    F1AP_NRCGI_t const* cgi = &item->value.choice.GNB_DU_Served_Cells_Item.served_Cell_Information.nRCGI;
    if (cell_id_from_bits(&cgi->nRCellIdentity, cell_id) && nr_cgi_bytes(cgi, nr_cgi))
      return true;
  }
  return false;
}

bool f1_setup_cell(byte_array_t req, uint64_t* cell_id, uint8_t nr_cgi[NR_CGI_LEN])
{
  if (req.buf == NULL || req.len == 0)
    return false;

  asn_codec_ctx_t st = {.max_stack_size = 100 * 1000};
  F1AP_F1AP_PDU_t* pdu = NULL;
  asn_dec_rval_t const rval = aper_decode(&st, &asn_DEF_F1AP_F1AP_PDU, (void**)&pdu, req.buf, req.len, 0, 0);
  bool found = false;
  if (rval.code == RC_OK && pdu->present == F1AP_F1AP_PDU_PR_initiatingMessage
      && pdu->choice.initiatingMessage->procedureCode == F1AP_ProcedureCode_id_F1Setup
      && pdu->choice.initiatingMessage->value.present == F1AP_InitiatingMessage__value_PR_F1SetupRequest) {
    found = served_cell(&pdu->choice.initiatingMessage->value.choice.F1SetupRequest, cell_id, nr_cgi);
  } else {
    fprintf(stderr, "[e2e_ctrl] F1 component config does not decode as F1 Setup Request\n");
  }
  if (pdu != NULL)
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return found;
}
