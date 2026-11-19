/* collect/kpm_values.c - KPM record walker. See kpm_values.h. */
#include "collect/kpm_values.h"

static bool record_value(meas_record_lst_t const* rec, double* v)
{
  if (rec->value == REAL_MEAS_VALUE) {
    *v = rec->real_val;
    return true;
  }
  if (rec->value == INTEGER_MEAS_VALUE) {
    *v = rec->int_val;
    return true;
  }
  return false;
}

void kpm_values_for_each(kpm_ind_msg_format_1_t const* f1, kpm_value_fn fn, void* ctx)
{
  for (size_t j = 0; j < f1->meas_data_lst_len; j++) {
    meas_data_lst_t const* data = &f1->meas_data_lst[j];
    size_t k = 0; /* record index runs over (measurement, label) pairs */
    for (size_t m = 0; m < f1->meas_info_lst_len; m++) {
      meas_info_format_1_lst_t const* info = &f1->meas_info_lst[m];
      for (size_t z = 0; z < info->label_info_lst_len; z++, k++) {
        if (k >= data->meas_record_len || info->meas_type.type != NAME_MEAS_TYPE)
          continue;
        double v;
        if (record_value(&data->meas_record_lst[k], &v))
          fn(ctx, info->meas_type.name, v);
      }
    }
  }
}
