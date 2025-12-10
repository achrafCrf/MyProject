
#pragma once
#include <stddef.h>

/* =======================
 * Defaults (compat)
 * ======================= */
#define DEFAULT_THRESHOLD    550.0
#define DEFAULT_TMS_MS       2000
#define DEFAULT_SAMPLES      10
#define DEFAULT_SLEEP_MS     10
#define DEFAULT_MODE         "current"   /* rétro-compatibilité */

/* =======================
 * Defaults (étendu A/V)
 * ======================= */
#define DEFAULT_THRESHOLD_A  DEFAULT_THRESHOLD
#define DEFAULT_THRESHOLD_V  230.0
#define DEFAULT_TMS_A_MS     DEFAULT_TMS_MS
#define DEFAULT_TMS_V_MS     2000
#define DEFAULT_TRIP_LOGIC   "any"       /* "any" | "both" */

/* =======================
 * API publique
 * ======================= */

/* Charge config.json ; retourne 0 si OK, -1 si échec (valeurs par défaut utilisées). */
int   config_load(const char *path);

/* --- Getters "historiques" (compat main/mms existants) --- */
double      config_get_threshold(void);           /* retourne le seuil courant "générique" (par ex. A) */
int         config_get_tms_ms(void);              /* TMS "générique" (par ex. A) */
int         config_get_samples(void);
int         config_get_sleep_ms(void);
const char* config_get_mode(void);

/* --- Getters étendus (A/V + logique de déclenchement) --- */
double      config_get_threshold_A(void);
double      config_get_threshold_V(void);
int         config_get_tms_A_ms(void);
int         config_get_tms_V_ms(void);
const char* config_get_trip_logic(void);
