/* state/cu_meas.c - latest RRC measurement per UE. See cu_meas.h. */
#include "state/cu_meas.h"

void cu_meas_update(cu_meas_t* m, meas_report_t const* r, int64_t now_us)
{
  if (!r->has_srv)
    return;
  m->has_srv = true;
  m->srv = r->srv;
  m->srv_t_us = now_us;

  /* new list: insertion sort, strongest first, keep MAX_NB; equal RSRP keeps the report order */
  size_t n = 0;
  cell_rsrp_t nb[MAX_NB];
  for (size_t i = 0; i < r->n_nb && i < REPORT_MAX_NB; i++) {
    cell_rsrp_t const c = r->nb[i];
    if (c.pci == r->srv.pci) {
      m->n_srv_as_nb++;
      continue;
    }
    size_t k = n;
    while (k > 0 && nb[k - 1].rsrp_dbm < c.rsrp_dbm)
      k--;
    if (k == MAX_NB)
      continue; /* weaker than the MAX_NB kept */
    size_t const last = n < MAX_NB ? n : MAX_NB - 1;
    for (size_t j = last; j > k; j--)
      nb[j] = nb[j - 1];
    nb[k] = c;
    if (n < MAX_NB)
      n++;
  }
  for (size_t i = 0; i < n; i++)
    m->nb[i] = nb[i];
  m->n_nb = n;
  m->nb_known = true;
  m->nb_t_us = now_us;
  m->n_reports++;
}
