/* collect/kpm_values.h - walk the (measurement name, value) pairs of one KPM report.
 *
 * A KPM format-1 message stores names and values in two parallel lists (one record per
 * measurement and label). This hides that layout from kpm_du.c and kpm_cu.c.
 */
#ifndef E2E_CTRL_KPM_VALUES_H
#define E2E_CTRL_KPM_VALUES_H

#include "xApp/e42_xapp_api.h"

/* Called once per numeric value; ctx is passed through unchanged. */
typedef void (*kpm_value_fn)(void* ctx, byte_array_t name, double value);

void kpm_values_for_each(kpm_ind_msg_format_1_t const* f1, kpm_value_fn fn, void* ctx);

#endif
