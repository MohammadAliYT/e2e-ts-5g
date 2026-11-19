/* e2/f1_setup.h - read a DU's cell identity from its F1 Setup Request.
 *
 * With E2AP v2/v3 each E2 node sends its E2 Node Component Configuration; for a DU
 * the F1 component carries the F1 Setup Request (APER-encoded), which lists the
 * DU's served cells.
 */
#ifndef E2E_CTRL_F1_SETUP_H
#define E2E_CTRL_F1_SETUP_H

#include <stdbool.h>
#include <stdint.h>

#include "util/byte_array.h"

/* 36-bit NR Cell Identity of the first served cell. False if req is not an F1 Setup Request. */
bool f1_setup_cell_id(byte_array_t req, uint64_t* cell_id);

#endif
