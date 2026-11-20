/* e2/f1_setup.h - read a DU's cell identity from its F1 Setup Request.
 *
 * With E2AP v2/v3 each E2 node sends its E2 Node Component Configuration; for a DU
 * the F1 component carries the F1 Setup Request (APER-encoded), which lists the
 * DU's served cells. The NR CGI (PLMN + NR Cell Identity, 8 octets) is the target
 * cell of an RC handover (phase 3).
 */
#ifndef E2E_CTRL_F1_SETUP_H
#define E2E_CTRL_F1_SETUP_H

#include <stdbool.h>
#include <stdint.h>

#include "util/byte_array.h"

/* NR CGI = 3-octet PLMN Identity + 36-bit NR Cell Identity left-aligned in 5 octets
 * (TS 38.473 9.3.1.12). Same layout as decode_nr_cgi() in xapp_rc_handover.c. */
enum { NR_CGI_LEN = 8 };

/* First served cell: 36-bit NR Cell Identity and the raw NR CGI.
 * False if req is not an F1 Setup Request or lists no cell. */
bool f1_setup_cell(byte_array_t req, uint64_t* cell_id, uint8_t nr_cgi[NR_CGI_LEN]);

#endif
