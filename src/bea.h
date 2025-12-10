
// src/bea.h
#pragma once
#include <gpiod.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BEA-like: acquisition "analogique" basée sur HC-SR04 (pulse width en µs),
 * puis mapping vers "courant" (A) ou "tension" (V) via une calibration linéaire.
 *
 * On garde tes lignes:
 *  - TRIG = line 20 (P9_41)
 *  - ECHO = line 16 (P9_15)
 */

#define BEA_TRIG_LINE 20  // P9_41
#define BEA_ECHO_LINE 16  // P9_15

typedef struct {
    struct gpiod_line *trig;
    struct gpiod_line *echo;
    // Calibration linéaire: value = scale * pulse_us + offset
    double scale_current;   // A / µs
    double offset_current;  // A
    double scale_voltage;   // V / µs
    double offset_voltage;  // V
} bea_t;

/** Initialise TRIG/ECHO en sortie/entrée et charge une calibration par défaut. */
int bea_init(struct gpiod_chip *chip, bea_t *bea);

/** Met à jour la calibration courant. */
void bea_set_current_calib(bea_t *bea, double scale_A_per_us, double offset_A);

/** Met à jour la calibration tension. */
void bea_set_voltage_calib(bea_t *bea, double scale_V_per_us, double offset_V);

/** Mesure brute: durée du pulse ECHO en microsecondes (retourne <0 si erreur/timeout). */
double bea_measure_pulse_us(bea_t *bea);

/** Convertit la mesure en courant (A) via la calibration. */
double bea_sample_current_A(bea_t *bea);

/** Convertit la mesure en tension (V) via la calibration. */
double bea_sample_voltage_V(bea_t *bea);

/** Calcule le RMS sur N échantillons (courant) avec un pas (usleep) entre samples (µs). */
double bea_rms_current_A(bea_t *bea, int samples, int sleep_between_samples_us);

/** Calcule le RMS sur N échantillons (tension). */
double bea_rms_voltage_V(bea_t *bea, int samples, int sleep_between_samples_us);

#ifdef __cplusplus
}
#endif
