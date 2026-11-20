/* state/ue_view.c - per-CU-UE-ID view: serving DU, RNTI re-map, activity, medians. See ue_view.h. */
#include "state/ue_view.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static ue_view_t g_view[MAX_UE];

static ue_view_t* find(uint32_t id)
{
  for (size_t i = 0; i < MAX_UE; i++)
    if (g_view[i].used && g_view[i].cu_ue_id == id)
      return &g_view[i];
  return NULL;
}

static ue_view_t* get(uint32_t id)
{
  ue_view_t* v = find(id);
  if (v != NULL)
    return v;
  for (size_t i = 0; i < MAX_UE; i++) {
    if (!g_view[i].used) {
      v = &g_view[i];
      memset(v, 0, sizeof(*v));
      v->used = true;
      v->cu_ue_id = id;
      v->serving_di = -1;
      return v;
    }
  }
  fprintf(stderr, "[state] view full, CU UE ID %u ignored\n", id);
  return NULL;
}

/* Copy the window values of DU entry u (same rules as output/csv_row.c) */
static ue_view_win_t window_of(size_t di, ue_t const* u)
{
  ue_view_win_t w = {.di = (int)di, .rnti = u->rnti, .last_seen_us = u->last_seen_us};
  w.has_dl = u->w_dl_dt_us > 0;
  w.has_ul = u->w_ul_dt_us > 0;
  w.dl_mbps = w.has_dl ? (double)u->w_dl_bytes * 8.0 / (double)u->w_dl_dt_us : 0.0;
  w.ul_mbps = w.has_ul ? (double)u->w_ul_bytes * 8.0 / (double)u->w_ul_dt_us : 0.0;
  w.has_mac = u->w_n_mac > 0;
  if (w.has_mac) {
    double const n = u->w_n_mac;
    w.dl_mcs = u->w_dl_mcs / n;
    w.ul_mcs = u->w_ul_mcs / n;
    w.dl_bler = u->w_dl_bler / n;
    w.ul_bler = u->w_ul_bler / n;
  }
  w.has_kpm = u->w_n_kpm > 0;
  w.kpm_dl_kbps = u->w_kpm_dl_thp;
  return w;
}

void ue_view_begin_window(void)
{
  for (size_t i = 0; i < MAX_UE; i++) {
    g_view[i].updated = false;
    g_view[i].has_cand = false;
  }
}

void ue_view_add(size_t di, ue_t const* u)
{
  if (!u->has_cu_ue_id)
    return;
  ue_view_t* v = get(u->cu_ue_id);
  if (v == NULL)
    return;
  if (v->has_cand && v->cand.last_seen_us >= u->last_seen_us)
    return; /* the other DU entry has a newer report */
  v->cand = window_of(di, u);
  v->has_cand = true;
}

static void clear_radio(ue_view_t* v)
{
  med_clear(&v->dl_mcs);
  med_clear(&v->ul_mcs);
  med_clear(&v->dl_bler);
  med_clear(&v->ul_bler);
}

/* Serving DU and RNTI from this window's DU entry */
static void update_location(ue_view_t* v, ue_view_win_t const* w, int64_t t_rel)
{
  if (v->serving_di < 0) {
    fprintf(stderr, "[state] t=%" PRId64 " CU UE ID %u: new, serving DU%d RNTI %04x\n", t_rel, v->cu_ue_id, w->di + 1,
            w->rnti);
  } else if (w->di != v->serving_di) {
    v->ho_count++;
    fprintf(stderr, "[state] t=%" PRId64 " CU UE ID %u: HANDOVER DU%d RNTI %04x -> DU%d RNTI %04x (%u s on old DU, handover %u)\n",
            t_rel, v->cu_ue_id, v->serving_di + 1, v->rnti, w->di + 1, w->rnti, v->win_on_du, v->ho_count);
    clear_radio(v); /* radio values belong to the old cell */
    v->win_on_du = 0;
  } else if (w->rnti != v->rnti) {
    fprintf(stderr, "[state] t=%" PRId64 " CU UE ID %u: RNTI change on DU%d %04x -> %04x\n", t_rel, v->cu_ue_id, w->di + 1,
            v->rnti, w->rnti);
  }
  v->serving_di = w->di;
  v->rnti = w->rnti;
  v->win_on_du++;
}

/* Active/idle with hysteresis. Returns true if this window had traffic. */
static bool update_activity(ue_view_t* v, ue_view_win_t const* w, int64_t t_rel)
{
  bool const traffic = (w->dl_mbps + w->ul_mbps) >= ACTIVE_MBPS;
  if (traffic) {
    v->low_windows = 0;
    if (!v->active)
      fprintf(stderr, "[state] t=%" PRId64 " CU UE ID %u: active\n", t_rel, v->cu_ue_id);
    v->active = true;
  } else {
    v->low_windows++;
    if (v->active && v->low_windows >= IDLE_WINDOWS) {
      v->active = false;
      fprintf(stderr, "[state] t=%" PRId64 " CU UE ID %u: idle\n", t_rel, v->cu_ue_id);
    }
  }
  return traffic;
}

static void update_medians(ue_view_t* v, ue_view_win_t const* w, bool traffic)
{
  if (w->has_dl)
    med_push(&v->dl_mbps, w->dl_mbps);
  if (w->has_ul)
    med_push(&v->ul_mbps, w->ul_mbps);
  if (w->has_kpm)
    med_push(&v->kpm_dl_kbps, w->kpm_dl_kbps);
  /* MCS 0 without traffic is not a bad link: use radio values only from windows with traffic */
  if (w->has_mac && traffic) {
    med_push(&v->dl_mcs, w->dl_mcs);
    med_push(&v->ul_mcs, w->ul_mcs);
    med_push(&v->dl_bler, w->dl_bler);
    med_push(&v->ul_bler, w->ul_bler);
  }
}

void ue_view_end_window(int64_t t_rel, int64_t now_us)
{
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_view_t* v = &g_view[i];
    if (!v->used)
      continue;
    if (!v->has_cand) {
      if (now_us - v->last_seen_us > UE_TIMEOUT_US) {
        fprintf(stderr, "[state] t=%" PRId64 " CU UE ID %u: gone\n", t_rel, v->cu_ue_id);
        v->used = false;
      }
      continue;
    }
    ue_view_win_t const* w = &v->cand;
    v->last_seen_us = w->last_seen_us;
    update_location(v, w, t_rel);
    bool const traffic = update_activity(v, w, t_rel);
    update_medians(v, w, traffic);
    v->updated = true;
  }
}

ue_view_t const* ue_view_slot(size_t i) { return &g_view[i < MAX_UE ? i : 0]; }

ue_view_t const* ue_view_find(uint32_t cu_ue_id) { return find(cu_ue_id); }
