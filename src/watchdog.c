
// src/watchdog.c
#include "watchdog.h"
#include <time.h>
#include <stdio.h>

static int g_timeout_ms = WATCHDOG_TIMEOUT_MS_DEFAULT;
static struct timespec g_last_kick = {0};
static int g_fault = 0;

static inline int64_t ts_diff_ms(const struct timespec *a, const struct timespec *b) {
    int64_t s  = (int64_t)a->tv_sec  - (int64_t)b->tv_sec;
    int64_t ns = (int64_t)a->tv_nsec - (int64_t)b->tv_nsec;
    return s * 1000 + ns / 1000000;
}

int watchdog_init(int timeout_ms) {
    if (timeout_ms <= 0) {
        fprintf(stderr, "[WARN] watchdog_init: timeout invalide (%d ms). Utilisation de %d ms.\n",
                timeout_ms, WATCHDOG_TIMEOUT_MS_DEFAULT);
        g_timeout_ms = WATCHDOG_TIMEOUT_MS_DEFAULT;
    } else {
        g_timeout_ms = timeout_ms;
    }
    clock_gettime(CLOCK_MONOTONIC, &g_last_kick);
    g_fault = 0;
    fprintf(stdout, "[INFO] Watchdog initialisé avec timeout=%d ms.\n", g_timeout_ms);
    return 0;
}

void watchdog_kick(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_last_kick);
    // On ne log pas ici pour éviter le bruit. En cas de debug, on peut ajouter un trace.
}

int watchdog_check(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t elapsed = ts_diff_ms(&now, &g_last_kick);
    if (elapsed > g_timeout_ms) {
        if (!g_fault) {
            // Première détection de faute
            g_fault = 1;
        }
        return 1; // FAULT
    }
    g_fault = 0;
    return 0; // OK
}

int watchdog_is_fault(void) {
    return g_fault;
}

int watchdog_get_timeout_ms(void) {
    return g_timeout_ms;
}
