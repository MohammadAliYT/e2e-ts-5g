/* act/act.c - handover requests: check, send, confirm. See act.h. */
#include "act/act.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "act/cmd_fifo.h"
#include "config.h"
#include "e2/rc_handover.h"
#include "state/cu_table.h"
#include "state/median.h"
#include "state/ue_table.h"
#include "state/ue_view.h"
#include "util/time_now_us.h"

/* Last request per UE (pending until confirmed or timed out) */
typedef struct {
  bool used;
  bool pending;
  uint32_t cu_ue_id;
  int from_di, to_di;
  int64_t t_us;  /* time of the CONTROL */
} req_t;

static req_t g_req[MAX_UE];
static ran_nodes_t const* g_ran;
static bool g_ho_ok;

void act_init(ran_nodes_t const* ran)
{
  g_ran = ran;
  g_ho_ok = ran->has_cu && rc_handover_supported(&ran->cu);
  cmd_fifo_start();
}

void act_stop(void) { cmd_fifo_stop(); }

static req_t* req_find(uint32_t id, bool create)
{
  req_t* free_slot = NULL;
  for (size_t i = 0; i < MAX_UE; i++) {
    if (g_req[i].used && g_req[i].cu_ue_id == id)
      return &g_req[i];
    if (!g_req[i].used && free_slot == NULL)
      free_slot = &g_req[i];
  }
  if (create && free_slot != NULL) {
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->used = true;
    free_slot->cu_ue_id = id;
  }
  return create ? free_slot : NULL;
}

/* Step 1 (lock held): NULL if allowed, else the reason. Fills from_di and a copy of the UE ID. */
static const char* check(uint32_t id, int to_di, int64_t now, int* from_di, ue_id_e2sm_t* ue_id)
{
  if (!g_ho_ok)
    return "CU has no RC handover control";
  if (to_di < 0 || (size_t)to_di >= g_ran->n_du || !g_ran->du[to_di].has_cell)
    return "unknown target DU or no NR CGI for it";
  ue_view_t const* v = ue_view_find(id);
  if (v == NULL || v->serving_di < 0)
    return "UE not known";
  if (!v->updated)
    return "UE did not report in the last window";
  if (v->serving_di == to_di)
    return "UE is already on the target DU";
  req_t const* r = req_find(id, false);
  if (r != NULL && r->pending)
    return "a handover for this UE is pending";
  if (r != NULL && now - r->t_us < HO_COOLDOWN_S * 1000000LL)
    return "cooldown after the last handover request";
  cu_ue_t const* cu = cu_table_find(id);
  if (cu == NULL || !cu->has_ue_id)
    return "no CU KPM report for this UE yet (no UE ID for the CONTROL)";
  *from_di = v->serving_di;
  *ue_id = cp_ue_id_e2sm(&cu->ue_id);
  return NULL;
}

bool act_request(uint32_t id, int to_di, int64_t t_rel)
{
  int64_t const now = time_now_us();
  int from_di = -1;
  ue_id_e2sm_t ue_id = {0};

  ue_table_lock();
  const char* why = check(id, to_di, now, &from_di, &ue_id);
  ue_table_unlock();
  if (why != NULL) {
    fprintf(stderr, "[act] t=%" PRId64 " handover CU UE ID %u -> DU%d REFUSED: %s\n", t_rel, id, to_di + 1, why);
    return false;
  }

  fprintf(stderr, "[act] t=%" PRId64 " handover CU UE ID %u DU%d -> DU%d (NR cell ID %" PRIu64 "): CONTROL sent\n", t_rel, id,
          from_di + 1, to_di + 1, g_ran->du[to_di].nr_cell_id);
  rc_handover_send(&g_ran->cu, &ue_id, g_ran->du[to_di].nr_cgi); /* blocks until the CU answers */
  free_ue_id_e2sm(&ue_id);
  int64_t const t_ack = time_now_us();
  fprintf(stderr, "[act] t=%" PRId64 " handover CU UE ID %u: CU answered after %.0f ms\n", t_rel, id, (t_ack - now) / 1000.0);

  ue_table_lock();
  req_t* r = req_find(id, true);
  if (r != NULL) {
    r->pending = true;
    r->from_di = from_di;
    r->to_di = to_di;
    r->t_us = now;
  }
  ue_table_unlock();
  return true;
}

/* "show": one line per UE in the view (lock NOT held on entry) */
static void show(int64_t t_rel)
{
  ue_table_lock();
  for (size_t i = 0; i < MAX_UE; i++) {
    ue_view_t const* v = ue_view_slot(i);
    if (!v->used)
      continue;
    double dl = 0, ul = 0, mcs = -1;
    med_get(&v->dl_mbps, &dl);
    med_get(&v->ul_mbps, &ul);
    med_get(&v->dl_mcs, &mcs);
    fprintf(stderr, "[act] t=%" PRId64 " show: CU UE ID %u on DU%d RNTI %04x %s, %u s on DU, %u handovers, dl %.2f ul %.2f Mbit/s, dl_mcs %.1f\n",
            t_rel, v->cu_ue_id, v->serving_di + 1, v->rnti, v->active ? "active" : "idle", v->win_on_du, v->ho_count, dl, ul,
            mcs);
  }
  ue_table_unlock();
}

void act_run_commands(int64_t t_rel)
{
  cmd_t cmds[8];
  size_t const n = cmd_fifo_take(cmds, 8);
  for (size_t i = 0; i < n; i++) {
    if (cmds[i].type == CMD_SHOW)
      show(t_rel);
    else
      act_request(cmds[i].cu_ue_id, cmds[i].du_no - 1, t_rel);
  }
}

void act_check_pending(int64_t t_rel, int64_t now_us)
{
  for (size_t i = 0; i < MAX_UE; i++) {
    req_t* r = &g_req[i];
    if (!r->used || !r->pending)
      continue;
    double const age_s = (now_us - r->t_us) / 1e6;
    ue_view_t const* v = ue_view_find(r->cu_ue_id);
    if (v != NULL && v->serving_di == r->to_di) {
      fprintf(stderr, "[act] t=%" PRId64 " handover CU UE ID %u -> DU%d confirmed after %.1f s (new RNTI %04x)\n", t_rel,
              r->cu_ue_id, r->to_di + 1, age_s, v->rnti);
      r->pending = false;
    } else if (age_s > HO_CONFIRM_S) {
      fprintf(stderr, "[act] t=%" PRId64 " handover CU UE ID %u -> DU%d NOT confirmed after %d s (serving DU%d)\n", t_rel,
              r->cu_ue_id, r->to_di + 1, HO_CONFIRM_S, v != NULL ? v->serving_di + 1 : 0);
      r->pending = false;
    }
  }
}
