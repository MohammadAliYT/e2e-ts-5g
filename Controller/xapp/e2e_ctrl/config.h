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
enum { RF_MAC = 142, RF_RLC = 143, RF_KPM = 2, RF_RC = 3 };

/* NEVER subscribe these on the CU: GTP (148) crashed the CU on 19 Nov (read_gtp_sm asserts one
 * QoS flow per PDU session, the lab has 2 DRBs); PDCP (144) not checked for the same problem.
 * e2/subscribe_cu.c refuses them. */
enum { RF_PDCP = 144, RF_GTP = 148 };

/* Report periods. MAC/RLC accept only "1_ms", "2_ms", "5_ms", "10_ms" (mac_sm_ric.c, rlc_sm_ric.c) */
#define MAC_INTERVAL "10_ms"
#define RLC_INTERVAL "10_ms"
#define KPM_PERIOD_MS 1000u

/* Lab S-NSSAI (slice): SST 1, SD 0x010203 (same filter as the patched xapp_kpm_moni) */
#define SNSSAI_SST 1
#define SNSSAI_SD_INIT {0x01, 0x02, 0x03}

/* RLC: OAI reports DRB rb_id 1..5. The lab has 2 DRBs per PDU session, and which one carries the DL or UL
 * data is not fixed (it changed after a re-establishment, 23 Nov), so collect/rlc.c sums all DRBs. */
enum { MAX_RB = 6 };

/* Start-up guard: wait until the RIC lists this many DUs (from $E2E_DUS, else DUS_DEFAULT) and a CU,
 * at most NODES_WAIT_S seconds; else stop. The node list is read only once, so a controller started
 * before every E2 setup would run without the missing nodes (20 Nov 20:09: no DU2). */
#define DUS_ENV "E2E_DUS"
enum { DUS_DEFAULT = 2, NODES_WAIT_S = 30 };

/* Table sizes */
enum { MAX_DU = 4, MAX_UE = 32 };

/* Drop a (DU, RNTI) entry after this long without any report */
#define UE_TIMEOUT_US (5 * 1000000LL)

/* CSV output: path from this environment variable, else e2e_ctrl_<date>_<time>.csv */
#define CSV_PATH_ENV "E2E_CSV"

/* ---- phase 2: state ---- */
/* Medians use the last MED_WINDOWS windows (1 s each) */
#define MED_WINDOWS 5
/* A UE has traffic in a window when RLC DL + UL >= ACTIVE_MBPS (Mbit/s).
 * 0.1 Mbit/s: a ping every second (~0.001 Mbit/s) is idle; iperf3 at 1 Mbit/s is active. */
#define ACTIVE_MBPS 0.1
/* An active UE becomes idle after IDLE_WINDOWS windows in a row without traffic */
#define IDLE_WINDOWS 3
/* State CSV: path from this environment variable, else the main CSV path with "_state" */
#define STATE_CSV_PATH_ENV "E2E_STATE_CSV"

/* ---- phase 3: act on command ---- */
/* Command FIFO: path from this environment variable, else CMD_FIFO_DEFAULT */
#define CMD_FIFO_ENV "E2E_CMD"
#define CMD_FIFO_DEFAULT "/tmp/e2e_ctrl.cmd"
/* No new handover request for a UE until this long after the last one (no ping-pong) */
#define HO_COOLDOWN_S 5
/* A handover is "NOT confirmed" when the UE view does not show the target DU after this long */
#define HO_CONFIRM_S 10

/* ---- phase 1b: RRC measurements (RC REPORT style 1 = a copy of every UE MeasurementReport) ---- */
/* E2SM-RC REPORT style 1 "Message Copy"; RAN parameters 3 "RRC Message" and 4 "UE ID" (rc_data_ie.h).
 * The CU adds the UE ID only with Controller/patches/oai-cu-rc-msg-copy-ueid.patch. */
enum { RC_REPORT_STYLE_MSG_COPY = 1 };
/* UL-DCCH message ID of a MeasurementReport: the CU compares c1->present (rrc_gNB.c:2391), i.e.
 * NR_UL_DCCH_MessageType__c1_PR_measurementReport (checked by a static assert in collect/meas_report_dec.c) */
enum { RRC_MSG_ID_MEAS_REPORT = 1 };
/* Event Trigger Condition ID of that trigger (any value; the CU sends it back in the indication header) */
enum { RC_MEAS_COND_ID = 1 };
/* Neighbours kept per UE, strongest first; the CSV shows the strongest */
enum { MAX_NB = 4 };
/* RSRP-Range index n -> dBm: -157 + n <= RSRP < -156 + n, so dBm = n - 157 is the lower edge of the
 * 1 dB bin = the UE's value (oai-ue-rrc-rsrp-report.patch: index = RSRP + 157).
 * The CU's nrRRC_stats.log prints n - 156 (upper edge), i.e. 1 dB more. */
#define RSRP_INDEX_TO_DBM 157

#endif
