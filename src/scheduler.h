
// src/scheduler.h
#pragma once
#include <time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * APS-like: ordonnancement de tâches périodiques avec offset.
 * - Chaque "task" a une période (ms) et un offset initial (ms).
 * - Le scheduler exécute les callbacks dans un thread appelant (boucle bloquante).
 */

typedef void (*aps_task_fn_t)(void *ctx);

typedef struct {
    aps_task_fn_t fn;       // Callback à exécuter
    void *ctx;              // Contexte passé au callback
    uint32_t period_ms;     // Période en millisecondes
    uint32_t offset_ms;     // Décalage initial en millisecondes
    struct timespec last_ts;// Dernière exécution
    int offset_applied;     // 0 = pas encore appliqué, 1 = offset déjà consommé
} aps_task_t;

typedef struct {
    aps_task_t *tasks;
    int max_tasks;
    int count;
    struct timespec start_ts;
} aps_scheduler_t;

/** Initialise le scheduler avec un tableau de tâches pré-alloué. */
void aps_init(aps_scheduler_t *sch, aps_task_t *tasks_buf, int max_tasks);

/** Ajoute une tâche (retourne 0 si OK, -1 si plein). */
int aps_add_task(aps_scheduler_t *sch, aps_task_fn_t fn, void *ctx,
                 uint32_t period_ms, uint32_t offset_ms);

/** Boucle d’exécution bloquante (appelle les callbacks quand ils sont dus). */
void aps_run(aps_scheduler_t *sch);

/** Outil: différence (ms) entre deux timespec. */
static inline int64_t ts_diff_ms(const struct timespec *a, const struct timespec *b) {
    int64_t s = (int64_t)a->tv_sec - (int64_t)b->tv_sec;
    int64_t ns = (int64_t)a->tv_nsec - (int64_t)b->tv_nsec;
    return s * 1000 + ns / 1000000;
}

#ifdef __cplusplus
}
#endif
