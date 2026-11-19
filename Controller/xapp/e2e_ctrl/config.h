/* config.h - every fixed setting of the controller in one place.
 *
 * Change values here, not in the modules. Later phases replace some of these
 * with a config file (phase 9).
 */
#ifndef E2E_CTRL_CONFIG_H
#define E2E_CTRL_CONFIG_H

#if !defined(E2AP_V2) && !defined(E2AP_V3)
#error "e2e_ctrl needs E2AP v2 or v3 (E2 Node Component Config carries the F1 Setup Request)"
#endif

/* RAN function IDs (service models):
 * src/sm/mac_sm/mac_sm_id.h, rlc_sm/rlc_sm_id.h, kpm_sm/kpm_sm_v02.03/kpm_sm_id.h */
enum { RF_MAC = 142, RF_RLC = 143, RF_KPM = 2 };

/* Report periods. MAC/RLC accept only "1_ms", "2_ms", "5_ms", "10_ms" (mac_sm_ric.c, rlc_sm_ric.c) */
#define MAC_INTERVAL "10_ms"
#define RLC_INTERVAL "10_ms"
#define KPM_PERIOD_MS 1000u

/* Lab S-NSSAI (slice): SST 1, SD 0x010203 (same filter as the patched xapp_kpm_moni) */
#define SNSSAI_SST 1
#define SNSSAI_SD_INIT {0x01, 0x02, 0x03}

/* Per PDU session there are 2 DRBs: DL data on DRB 1, UL data on DRB 2 (RLC counters, 19 Nov).
 * OAI reports rb_id 1..5. */
enum { DL_DRB = 1, UL_DRB = 2, MAX_RB = 6 };

/* Table sizes */
enum { MAX_DU = 4, MAX_UE = 32 };

/* Drop a (DU, RNTI) entry after this long without any report */
#define UE_TIMEOUT_US (5 * 1000000LL)

/* CSV output: path from this environment variable, else e2e_ctrl_<date>_<time>.csv */
#define CSV_PATH_ENV "E2E_CSV"

#endif
