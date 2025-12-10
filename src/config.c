
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* =======================
 * Stockage interne
 * ======================= */

/* Étendu */
static double thr_A    = DEFAULT_THRESHOLD_A;
static double thr_V    = DEFAULT_THRESHOLD_V;
static int    tms_A    = DEFAULT_TMS_A_MS;
static int    tms_V    = DEFAULT_TMS_V_MS;

/* Commun */
static int    samples  = DEFAULT_SAMPLES;
static int    sleep_ms = DEFAULT_SLEEP_MS;

/* Compat historique */
static char   mode[16] = DEFAULT_MODE;

/* Logique de déclenchement (étendu) */
static char   trip_logic[8] = DEFAULT_TRIP_LOGIC;

/* =======================
 * Helpers internes
 * ======================= */

static char* trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) e--;
    e[1] = '\0';
    return s;
}

static char* unquote(char *s) {
    s = trim(s);
    size_t n = s ? strlen(s) : 0;
    if (n >= 2 && s[0] == '\"' && s[n-1] == '\"') {
        s[n-1] = '\0';
        return s + 1;
    }
    return s;
}

/* Applique des bornes raisonnables pour éviter valeurs aberrantes. */
static void clamp_all(void) {
    if (thr_A < 0.0)    thr_A = DEFAULT_THRESHOLD_A;
    if (thr_V < 0.0)    thr_V = DEFAULT_THRESHOLD_V;
    if (tms_A <= 0)     tms_A = DEFAULT_TMS_A_MS;
    if (tms_V <= 0)     tms_V = DEFAULT_TMS_V_MS;
    if (samples <= 0 || samples > 128) samples = DEFAULT_SAMPLES;
    if (sleep_ms < 0 || sleep_ms > 1000) sleep_ms = DEFAULT_SLEEP_MS;

    if (strcmp(trip_logic, "any") != 0 && strcmp(trip_logic, "both") != 0) {
        strncpy(trip_logic, DEFAULT_TRIP_LOGIC, sizeof(trip_logic)-1);
        trip_logic[sizeof(trip_logic)-1] = '\0';
    }
}

/* =======================
 * Chargement de config.json (parser minimal)
 * ======================= */

int config_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[WARN] Impossible d'ouvrir %s (%s). Valeurs par défaut.\n",
                path, strerror(errno));
        /* on garde les valeurs par défaut */
        clamp_all();
        return -1;
    }

    char line[512];


    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, ":");
        char *val = strtok(NULL, ",}\n");
        
        if (!key || !val) continue;
        key = trim(key);
        val = trim(val);
        /* ---- Étendu A/V ---- */
        if (strstr(key, "threshold_A")) { thr_A = atof(val);printf("[INFO] thr_A =%.2f\n",thr_A) ; continue; }
        if (strstr(key, "threshold_V")) { thr_V = atof(val); printf("[INFO] thr_V =%.2f\n",thr_V) ; continue; }
        if (strstr(key, "tms_A_ms"))    { tms_A = atoi(val); continue; }
        if (strstr(key, "tms_V_ms"))    { tms_V = atoi(val); continue; }
        if (strstr(key, "trip_logic"))  {
            val = unquote(val);
            strncpy(trip_logic, val, sizeof(trip_logic)-1);
            trip_logic[sizeof(trip_logic)-1] = '\0';
            continue;
        }

        /* ---- Commun ---- */
        if (strstr(key, "samples")) { samples = atoi(val);  continue; }
        if (strstr(key, "sleep_between_samples_ms")) { sleep_ms = atoi(val);  continue; }

        /* ---- Compat historique ---- */
        if (strstr(key, "threshold")) { thr_A = atof(val); continue; }   /* map -> A */
        if (strstr(key, "tms_ms"))    { tms_A = atoi(val);  continue; }  /* map -> A */
        if (strstr(key, "mode")) {
            val = unquote(val);
            strncpy(mode, val, sizeof(mode)-1);
            mode[sizeof(mode)-1] = '\0';
            continue;
        }
    }

    fclose(f);
    clamp_all();

    /* Diagnostic synthèse */
    fprintf(stdout,
        "[INFO] Config chargée: "
        "thrA=%.3f tmsA=%d thrV=%.3f tmsV=%d smp=%d sleep=%d mode=%s logic=%s\n",
        thr_A, tms_A, thr_V, tms_V, samples, sleep_ms, mode, trip_logic);

    /* Remarque: si seules les clés historiques ont été trouvées, c'est OK (A utilisera ces valeurs). */

    return 0;
}

/* =======================
 * Getters (compat historiques)
 * ======================= */

double config_get_threshold(void)    { return thr_A; }   /* on expose "A" comme seuil générique */
int    config_get_tms_ms(void)       { return tms_A; }   /* idem pour TMS générique (A) */
int    config_get_samples(void)      { return samples; }
int    config_get_sleep_ms(void)     { return sleep_ms; }
const char* config_get_mode(void)    { return mode; }

/* =======================
 * Getters étendus
 * ======================= */

double      config_get_threshold_A(void)  { return thr_A; }
double      config_get_threshold_V(void)  { return thr_V; }
int         config_get_tms_A_ms(void)     { return tms_A; }
int         config_get_tms_V_ms(void)     { return tms_V; }
const char* config_get_trip_logic(void)   { return trip_logic; }
