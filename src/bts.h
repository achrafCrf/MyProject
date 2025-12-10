
// src/bts.h
#pragma once
#include <gpiod.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BTS-like: contrôle des sorties TOR (LED rouge et verte).
 * - LED rouge = déclenchement
 * - LED verte = état normal
 */

#define LED_RED_LINE   23 // P8_13
#define LED_GREEN_LINE 22 // P8_19

typedef struct {
    struct gpiod_line *led_red;
    struct gpiod_line *led_green;
} bts_t;

/** Initialise les LEDs en sortie. */
int bts_init(struct gpiod_chip *chip, bts_t *bts);

/** Met l'état du "disjoncteur": 0 = normal (vert ON), 1 = déclenchement (rouge ON). */
void bts_set_state(bts_t *bts, int state);

#ifdef __cplusplus
}
#endif
