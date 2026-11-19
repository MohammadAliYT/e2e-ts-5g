/* output/csv_row.c - CSV header and row formatting. See csv_row.h. */
#include "output/csv_row.h"

#include <inttypes.h>
#include <math.h>

static const char CSV_HEADER[] =
    /* phase 0 */
    "t_unix,t_rel_s,du,nb_id,nr_cell_id,rnti,cu_ue_id,"
    "dl_mbps,ul_mbps,dl_mcs,ul_mcs,dl_bler,ul_bler,pusch_snr,pucch_snr,"
    "kpm_dl_thp_kbps,kpm_ul_thp_kbps,kpm_prb_dl,kpm_prb_ul,cell_prb_dl,cell_prb_ul,"
    "n_mac,n_rlc,n_kpm,rlc_resets,"
    /* phase 1a */
    "rlc_delay_dl_us,amf_ue_ngap_id,cu_pdcp_dl_mbit,cu_pdcp_ul_mbit,n_kpm_cu,"
    /* phase 1a, DL queue (RLC SM, DRB 1) */
    "dl_hol_us_mean,dl_hol_us_max,dl_buf_bytes_mean,dl_buf_bytes_max\n";

/* ",value" or just "," when the value is missing */
static void num(FILE* f, double v, bool valid, const char* fmt)
{
  fputc(',', f);
  if (valid && isfinite(v))
    fprintf(f, fmt, v);
}

static double mbps(uint64_t bytes, int64_t dt_us) { return dt_us > 0 ? (double)bytes * 8.0 / (double)dt_us : NAN; }

void csv_row_header(FILE* f) { fputs(CSV_HEADER, f); }

void csv_row_write(FILE* f, int64_t t_s, int64_t t_rel, size_t du_no, du_info_t const* du, ue_t const* u,
                   cell_window_t const* cell, cu_ue_t const* cu)
{
  double const nm = u->w_n_mac;
  bool const mac = u->w_n_mac > 0;
  bool const kpm = u->w_n_kpm > 0;
  bool const cu_kpm = cu != NULL && cu->w_n_kpm > 0;

  fprintf(f, "%" PRId64 ",%" PRId64 ",%zu,%u,", t_s, t_rel, du_no, du->nb_id);
  if (du->has_cell)
    fprintf(f, "%" PRIu64, du->nr_cell_id);
  fprintf(f, ",%04x,", u->rnti);
  if (u->has_cu_ue_id)
    fprintf(f, "%u", u->cu_ue_id);

  num(f, mbps(u->w_dl_bytes, u->w_dl_dt_us), u->w_dl_dt_us > 0, "%.3f");
  num(f, mbps(u->w_ul_bytes, u->w_ul_dt_us), u->w_ul_dt_us > 0, "%.3f");
  num(f, mac ? u->w_dl_mcs / nm : NAN, mac, "%.1f");
  num(f, mac ? u->w_ul_mcs / nm : NAN, mac, "%.1f");
  num(f, mac ? u->w_dl_bler / nm : NAN, mac, "%.4f");
  num(f, mac ? u->w_ul_bler / nm : NAN, mac, "%.4f");
  num(f, mac ? u->w_pusch_snr / nm : NAN, mac, "%.1f");
  num(f, mac ? u->w_pucch_snr / nm : NAN, mac, "%.1f");
  num(f, u->w_kpm_dl_thp, kpm, "%.0f");
  num(f, u->w_kpm_ul_thp, kpm, "%.1f");
  num(f, u->w_kpm_prb_dl, kpm, "%.0f");
  num(f, u->w_kpm_prb_ul, kpm, "%.0f");
  num(f, cell->prb_dl, cell->valid, "%.0f");
  num(f, cell->prb_ul, cell->valid, "%.0f");
  fprintf(f, ",%u,%u,%u,%u", u->w_n_mac, u->w_n_rlc, u->w_n_kpm, u->w_rlc_resets);

  num(f, u->w_kpm_rlc_delay_dl_us, kpm, "%.0f");
  fputc(',', f);
  if (cu != NULL)
    fprintf(f, "%" PRIu64, cu->amf_ue_ngap_id);
  num(f, cu_kpm ? cu->w_pdcp_dl_mbit : NAN, cu_kpm, "%.0f");
  num(f, cu_kpm ? cu->w_pdcp_ul_mbit : NAN, cu_kpm, "%.0f");
  fprintf(f, ",%u", cu != NULL ? cu->w_n_kpm : 0u);

  bool const q = u->w_n_q > 0;
  num(f, q ? u->w_dl_hol_us_sum / u->w_n_q : NAN, q, "%.0f");
  num(f, (double)u->w_dl_hol_us_max, q, "%.0f");
  num(f, q ? u->w_dl_buf_bytes_sum / u->w_n_q : NAN, q, "%.0f");
  num(f, (double)u->w_dl_buf_bytes_max, q, "%.0f");
  fputc('\n', f);
}

void csv_row_status(int64_t t_rel, size_t du_no, ue_t const* u, cu_ue_t const* cu)
{
  bool const mac = u->w_n_mac > 0;
  double const nm = u->w_n_mac;
  fprintf(stderr, "[e2e_ctrl] t=%" PRId64 " DU%zu rnti=%04x dl=%.2f ul=%.2f Mbit/s dl_mcs=%.0f ul_mcs=%.0f", t_rel, du_no,
          u->rnti, u->w_dl_dt_us > 0 ? mbps(u->w_dl_bytes, u->w_dl_dt_us) : 0.0,
          u->w_ul_dt_us > 0 ? mbps(u->w_ul_bytes, u->w_ul_dt_us) : 0.0, mac ? u->w_dl_mcs / nm : -1.0,
          mac ? u->w_ul_mcs / nm : -1.0);
  if (u->w_n_q > 0)
    fprintf(stderr, " hol=%.0f/%uus buf=%.0fB", u->w_dl_hol_us_sum / u->w_n_q, u->w_dl_hol_us_max,
            u->w_dl_buf_bytes_sum / u->w_n_q);
  if (cu != NULL && cu->w_n_kpm > 0)
    fprintf(stderr, " cu_pdcp=%.0f/%.0f Mbit", cu->w_pdcp_dl_mbit, cu->w_pdcp_ul_mbit);
  fputc('\n', stderr);
}
