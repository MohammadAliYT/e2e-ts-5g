/* collect/meas_report_dec.h - decode one UL-DCCH-Message (UPER, TS 38.331) with a MeasurementReport.
 *
 * Uses FlexRIC's NR RRC ASN.1 library (Rel 17.3.0, asn1_nr_rrc, already linked through e42_xapp),
 * the same decoder as examples/xApp/c/monitor/xapp_rc_moni.c. Reads the first serving MO entry and
 * the NR neighbour list, SSB RSRP only (the lab configures SSB reports).
 */
#ifndef E2E_CTRL_MEAS_REPORT_DEC_H
#define E2E_CTRL_MEAS_REPORT_DEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "state/cu_meas.h"

/* False if buf is no MeasurementReport or does not decode. out->has_srv is false if the report has no
 * serving cell with PCI and SSB RSRP. Neighbours without PCI or SSB RSRP are left out. */
bool meas_report_decode(uint8_t const* buf, size_t len, meas_report_t* out);

#endif
