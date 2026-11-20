/* collect/meas_report_dec.c - UL-DCCH MeasurementReport -> meas_report_t. See meas_report_dec.h. */
#include "collect/meas_report_dec.h"

#include <assert.h>

#include "NR_MeasurementReport.h"
#include "NR_UL-DCCH-Message.h"
#include "config.h"

static_assert((int)NR_UL_DCCH_MessageType__c1_PR_measurementReport == (int)RRC_MSG_ID_MEAS_REPORT,
              "config.h RRC_MSG_ID_MEAS_REPORT must be the UL-DCCH c1 index of MeasurementReport");

/* PCI and SSB RSRP of one MeasResultNR; false if one of them is missing */
static bool cell_of(NR_MeasResultNR_t const* r, cell_rsrp_t* c)
{
  NR_MeasQuantityResults_t const* q = r->measResult.cellResults.resultsSSB_Cell;
  if (r->physCellId == NULL || q == NULL || q->rsrp == NULL)
    return false;
  c->pci = (uint16_t)*r->physCellId;
  c->rsrp_dbm = (int)*q->rsrp - RSRP_INDEX_TO_DBM;
  return true;
}

static void fill(NR_MeasResults_t const* res, meas_report_t* out)
{
  if (res->measResultServingMOList.list.count > 0)
    out->has_srv = cell_of(&res->measResultServingMOList.list.array[0]->measResultServingCell, &out->srv);

  struct NR_MeasResults__measResultNeighCells const* nc = res->measResultNeighCells;
  if (nc == NULL || nc->present != NR_MeasResults__measResultNeighCells_PR_measResultListNR || nc->choice.measResultListNR == NULL)
    return; /* no NR neighbour in this report */
  for (int i = 0; i < nc->choice.measResultListNR->list.count && out->n_nb < REPORT_MAX_NB; i++)
    if (cell_of(nc->choice.measResultListNR->list.array[i], &out->nb[out->n_nb]))
      out->n_nb++;
}

bool meas_report_decode(uint8_t const* buf, size_t len, meas_report_t* out)
{
  *out = (meas_report_t){0};
  if (buf == NULL || len == 0)
    return false;

  NR_UL_DCCH_Message_t* msg = NULL;
  asn_dec_rval_t const rv = uper_decode(NULL, &asn_DEF_NR_UL_DCCH_Message, (void**)&msg, buf, len, 0, 0);
  bool ok = false;
  if (rv.code == RC_OK && msg->message.present == NR_UL_DCCH_MessageType_PR_c1 && msg->message.choice.c1 != NULL
      && msg->message.choice.c1->present == NR_UL_DCCH_MessageType__c1_PR_measurementReport) {
    NR_MeasurementReport_t const* mr = msg->message.choice.c1->choice.measurementReport;
    if (mr->criticalExtensions.present == NR_MeasurementReport__criticalExtensions_PR_measurementReport
        && mr->criticalExtensions.choice.measurementReport != NULL) {
      fill(&mr->criticalExtensions.choice.measurementReport->measResults, out);
      ok = true;
    }
  }
  if (msg != NULL)
    ASN_STRUCT_FREE(asn_DEF_NR_UL_DCCH_Message, msg);
  return ok;
}
