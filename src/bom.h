
// src/bom.h
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

/**
 * BOM-like: logique de calcul pour comparer RMS avec seuil,
 * gérer temporisation (TMS) et invalidité.
 */
typedef struct {
  double threshold;      // Seuil (A ou V)
  int    tms_ms;         // Temporisation en millisecondes
  int    invalid;        // 0 = OK, 1 = invalide
  struct timespec tms_start; // début de la temporisation (par instance)
} bom_t;

/** Initialise BOM avec seuil et temporisation. */
void bom_init(bom_t *bom, double threshold, int tms_ms);
/** Vérifie si la valeur dépasse le seuil (retourne 1 si dépassement). */
int  bom_check(bom_t *bom, double value);
/** Applique temporisation: retourne 1 si dépassement persistant >= tms_ms. */
int  bom_check_with_tms(bom_t *bom, double value);
/** Marque invalidité (ex: capteur HS). */
void bom_set_invalid(bom_t *bom, int invalid);

#ifdef __cplusplus
}
#endif
