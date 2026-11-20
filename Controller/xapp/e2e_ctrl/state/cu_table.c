/* state/cu_table.c - CU per-UE table. See cu_table.h. */
#include "state/cu_table.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

static cu_ue_t g_cu[MAX_UE];

cu_ue_t* cu_table_find(uint32_t id)
{
  for (size_t i = 0; i < MAX_UE; i++)
    if (g_cu[i].used && g_cu[i].cu_ue_id == id)
      return &g_cu[i];
  return NULL;
}

cu_ue_t* cu_table_get(uint32_t id)
{
  cu_ue_t* u = cu_table_find(id);
  if (u != NULL)
    return u;
  for (size_t i = 0; i < MAX_UE; i++) {
    if (!g_cu[i].used) {
      memset(&g_cu[i], 0, sizeof(g_cu[i]));
      g_cu[i].used = true;
      g_cu[i].cu_ue_id = id;
      fprintf(stderr, "[e2e_ctrl] CU: new UE, CU UE ID %u\n", id);
      return &g_cu[i];
    }
  }
  fprintf(stderr, "[e2e_ctrl] CU: UE table full, CU UE ID %u ignored\n", id);
  return NULL;
}

static void clear_ue_id(cu_ue_t* u)
{
  if (u->has_ue_id)
    free_ue_id_e2sm(&u->ue_id);
  u->has_ue_id = false;
}

void cu_table_set_ue_id(cu_ue_t* u, ue_id_e2sm_t const* id)
{
  clear_ue_id(u);
  u->ue_id = cp_ue_id_e2sm(id);
  u->has_ue_id = true;
}

void cu_table_new_window(int64_t now_us)
{
  for (size_t i = 0; i < MAX_UE; i++) {
    cu_ue_t* u = &g_cu[i];
    if (!u->used)
      continue;
    if (now_us - u->last_seen_us > UE_TIMEOUT_US) {
      fprintf(stderr, "[e2e_ctrl] CU: UE with CU UE ID %u gone\n", u->cu_ue_id);
      clear_ue_id(u);
      u->used = false;
      continue;
    }
    u->w_n_kpm = 0;
    u->w_pdcp_dl_mbit = u->w_pdcp_ul_mbit = 0;
  }
}
