
// src/scheduler.c
#define _POSIX_C_SOURCE 200809L

#include "scheduler.h"
#include <string.h>
#include <unistd.h>
#include <time.h>


static void ts_now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

void aps_init(aps_scheduler_t *sch, aps_task_t *tasks_buf, int max_tasks) {
    sch->tasks = tasks_buf;
    sch->max_tasks = max_tasks;
    sch->count = 0;
    ts_now(&sch->start_ts);
}

int aps_add_task(aps_scheduler_t *sch, aps_task_fn_t fn, void *ctx,
                 uint32_t period_ms, uint32_t offset_ms) {
    if (sch->count >= sch->max_tasks) return -1;
    aps_task_t *t = &sch->tasks[sch->count++];
    t->fn = fn;
    t->ctx = ctx;
    t->period_ms = period_ms;
    t->offset_ms = offset_ms;
    t->offset_applied = 0;
    // Initialise "last_ts" à start - period pour permettre une exécution dès que due.
    struct timespec start = sch->start_ts;
    // recule last_ts pour que (now - last_ts) ~ period au 1er passage
    t->last_ts = start;
    return 0;
}

void aps_run(aps_scheduler_t *sch) {
    // Boucle simple avec sleep court pour limiter l’usage CPU (1 ms).
    while (1) {
        struct timespec now;
        ts_now(&now);

        for (int i = 0; i < sch->count; ++i) {
            aps_task_t *t = &sch->tasks[i];

            // Gestion de l’offset initial (exécuté une seule fois)
            if (!t->offset_applied) {
                int64_t since_start_ms = ts_diff_ms(&now, &sch->start_ts);
                if ((int64_t)t->offset_ms <= since_start_ms) {
                        t->fn(t->ctx);
                        t->last_ts = now;
                        t->offset_applied = 1;
                }
                continue; // tant que l’offset n’est pas consommé, ne pas exécuter périodiquement
            }

            // Exécution périodique
            int64_t since_last_ms = ts_diff_ms(&now, &t->last_ts);
            if ((int64_t)t->period_ms <=since_last_ms ) {
                t->fn(t->ctx);
                ts_now(&t->last_ts);
            }
        }

        // Dors 1 ms pour ne pas tourner à vide
        usleep(1000);
    }
}
