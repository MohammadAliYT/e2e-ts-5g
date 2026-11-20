/* state/median.h - median of the last MED_WINDOWS values (one value per 1 s window).
 *
 * Use it to filter values that have spikes (for example KPM DRB.UEThpDl).
 * The controller must never act on one report. It acts on the median.
 */
#ifndef E2E_CTRL_MEDIAN_H
#define E2E_CTRL_MEDIAN_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef struct {
  double v[MED_WINDOWS]; /* ring buffer */
  uint8_t n;             /* number of values in the buffer (0..MED_WINDOWS) */
  uint8_t next;          /* index for the next value */
} med_t;

/* Remove all values. */
void med_clear(med_t* m);

/* Add one value. When the buffer is full, the oldest value goes out. */
void med_push(med_t* m, double v);

/* Median of the values in the buffer. Returns false if the buffer is empty. */
bool med_get(med_t const* m, double* out);

#endif
