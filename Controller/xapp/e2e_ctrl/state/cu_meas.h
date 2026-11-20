/* state/cu_meas.h - the latest RRC measurement of one UE (phase 1b).
 *
 * Source: the UE's MeasurementReports, copied by the CU (RC REPORT style 1, collect/rc_meas.c).
 * Each report replaces the neighbour list: a report without neighbours means "no neighbour"
 * (n_nb = 0, nb_known stays true), not "unknown". Values are SSB RSRP in dBm (config.h RSRP_INDEX_TO_DBM).
 * Owned by state/cu_table.h; same lock (ue_table_lock()).
 */
#ifndef E2E_CTRL_CU_MEAS_H
#define E2E_CTRL_CU_MEAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

typedef struct {
  uint16_t pci;
  int rsrp_dbm;
} cell_rsrp_t;

/* One decoded MeasurementReport (collect/meas_report_dec.c fills it) */
enum { REPORT_MAX_NB = 8 }; /* maxCellReport is 8 (TS 38.331); OAI sends at most maxReportCells */
typedef struct {
  bool has_srv;  /* serving cell with PCI and SSB RSRP */
  cell_rsrp_t srv;
  size_t n_nb;   /* NR neighbours with PCI and SSB RSRP, in report order */
  cell_rsrp_t nb[REPORT_MAX_NB];
} meas_report_t;

typedef struct {
  bool has_srv;
  cell_rsrp_t srv;
  int64_t srv_t_us;    /* time of the report that set srv */

  bool nb_known;       /* at least one report with a serving cell came */
  size_t n_nb;         /* 0 = the last report had no neighbour */
  cell_rsrp_t nb[MAX_NB]; /* strongest first */
  int64_t nb_t_us;     /* time of the report that set the list (also an empty list) */

  uint32_t n_reports;    /* reports used since start */
  uint32_t n_srv_as_nb;  /* neighbour entries dropped because they have the serving PCI */
} cu_meas_t;

/* Use report r (received at now_us). A report without a serving cell changes nothing.
 * Neighbours with the serving PCI are dropped: a report carries no SSB frequency per neighbour,
 * so this check is intra-frequency only (as in the lab: one SSB frequency). */
void cu_meas_update(cu_meas_t* m, meas_report_t const* r, int64_t now_us);

#endif
