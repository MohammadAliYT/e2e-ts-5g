/* state/cu_table.h - what the CU reports per UE, keyed by the CU UE ID.
 *
 * The CU knows a UE by its CU UE ID (the same number the DU KPM gives as gnb_cu_ue_f1ap),
 * so output/csv.c joins a DU row (RNTI) with the CU entry of the same CU UE ID.
 * Uses the same lock as state/ue_table.h: hold ue_table_lock() while calling these.
 */
#ifndef E2E_CTRL_CU_TABLE_H
#define E2E_CTRL_CU_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lib/sm/ie/ue_id.h"

typedef struct {
  bool used;
  uint32_t cu_ue_id;
  uint64_t amf_ue_ngap_id; /* the core's (AMF's) ID for this UE - for the core phases */
  int64_t last_seen_us;

  /* full UE ID as the CU reports it (gNB type, RAN UE ID = CU UE ID): the RC handover
   * CONTROL header carries it (phase 3). Owned by the table: set with cu_table_set_ue_id. */
  bool has_ue_id;
  ue_id_e2sm_t ue_id;

  /* ---- current 1 s window ---- */
  uint32_t w_n_kpm;
  double w_pdcp_dl_mbit, w_pdcp_ul_mbit; /* DRB.PdcpSduVolumeDL/UL, whole Mbit per period, last report */
} cu_ue_t;

/* Entry for CU UE ID id; created if new. NULL if the table is full. */
cu_ue_t* cu_table_get(uint32_t id);

/* Entry for CU UE ID id, or NULL if unknown (does not create). */
cu_ue_t* cu_table_find(uint32_t id);

/* Store a copy of the CU's UE ID in u (frees the previous copy). */
void cu_table_set_ue_id(cu_ue_t* u, ue_id_e2sm_t const* id);

/* Start a new window for every entry; free entries silent for more than UE_TIMEOUT_US. */
void cu_table_new_window(int64_t now_us);

#endif
