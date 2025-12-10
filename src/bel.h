
// src/bel.h
#pragma once
#include <gpiod.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BEL-like: lecture d’une entrée logique (TOR) avec anti-rebond logiciel.
 * Tu pourras passer n’importe quel line (ex. un bouton sur P8/P9).
 */

typedef struct {
    struct gpiod_line *in;
    int active_high; // 1 si logique active-high, 0 si active-low
} bel_t;

/** Initialise la ligne en entrée (active_high contrôle la polarité logique). */
int bel_init(struct gpiod_chip *chip, bel_t *bel, int line, int active_high);

/** Lecture brute (0/1) selon polarité. Retourne -1 si erreur. */
int bel_read(bel_t *bel);

/** Lecture avec "debounce": N samples espacés de 'us' microsecondes, majorité. */
int bel_read_debounced(bel_t *bel, int samples, int us_between_samples);

#ifdef __cplusplus
}
#endif
