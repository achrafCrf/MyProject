
// src/bom.c
#include "bom.h"
#include <stdint.h>
#include <time.h>

static inline int64_t ts_diff_ms(const struct timespec *a, const struct timespec *b) {
  int64_t s  = (int64_t)a->tv_sec  - (int64_t)b->tv_sec;
  int64_t ns = (int64_t)a->tv_nsec - (int64_t)b->tv_nsec;
  return s * 1000 + ns / 1000000;
}

void bom_init(bom_t *bom, double threshold, int tms_ms) {
  bom->threshold  = threshold;
  bom->tms_ms     = tms_ms;
  bom->invalid    = 0;
  bom->tms_start  = (struct timespec){0}; // reset au démarrage
}

int bom_check(bom_t *bom, double value) {
  if (bom->invalid) return 0;        // invalide => jamais de déclenchement
  return (value > bom->threshold) ? 1 : 0;
}

int bom_check_with_tms(bom_t *bom, double value) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  if (bom_check(bom, value)) {
    // printf("je suis la\n");
    if (bom->tms_start.tv_sec == 0) {
      // printf("je viens de commencer ici\n");
      bom->tms_start = now;          // début temporisation
    }
    // printf("je suis laa\n");
    int64_t elapsed = ts_diff_ms(&now, &bom->tms_start);
    // printf("elapsed(ms)=%lld, tms(ms)=%d\n", (long long)elapsed, bom->tms_ms);
    int result=(elapsed >= bom->tms_ms) ? 1 : 0;
    // printf("resultat finale est=%d\n",result);
    return result;
  } else {
    // printf("je suis laaa\n");
    bom->tms_start.tv_sec = 0;       // reset temporisation
    bom->tms_start.tv_nsec = 0;
    return 0;
  }
}

void bom_set_invalid(bom_t *bom, int invalid) {
  bom->invalid = invalid ? 1 : 0;
}
