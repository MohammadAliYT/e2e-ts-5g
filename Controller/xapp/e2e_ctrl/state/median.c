/* state/median.c - ring buffer with median. See median.h. */
#include "state/median.h"

#include <string.h>

void med_clear(med_t* m) { memset(m, 0, sizeof(*m)); }

void med_push(med_t* m, double v)
{
  m->v[m->next] = v;
  m->next = (uint8_t)((m->next + 1) % MED_WINDOWS);
  if (m->n < MED_WINDOWS)
    m->n++;
}

bool med_get(med_t const* m, double* out)
{
  if (m->n == 0)
    return false;

  /* Sort a copy (insertion sort, at most MED_WINDOWS values) */
  double s[MED_WINDOWS];
  for (uint8_t i = 0; i < m->n; i++) {
    double const x = m->v[i];
    int j = i - 1;
    while (j >= 0 && s[j] > x) {
      s[j + 1] = s[j];
      j--;
    }
    s[j + 1] = x;
  }

  /* Odd count: middle value. Even count: mean of the two middle values. */
  uint8_t const h = m->n / 2;
  *out = (m->n % 2 == 1) ? s[h] : (s[h - 1] + s[h]) / 2.0;
  return true;
}
