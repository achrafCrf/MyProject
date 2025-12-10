
// src/main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gpiod.h>

#include "scheduler.h"
#include "bea.h"
#include "bel.h"
#include "bom.h"
#include "bts.h"
#include "mms.h"
#include "config.h"
#include "ArkStudio.h"
#include "watchdog.h"

#define CHIP "/dev/gpiochip0"

/* Active/désactive le mode simulation pour la démo */
#define SIMULATION 1

/* ----------- Globals ----------- */
static bea_t bea;
static bel_t bel;
static bts_t bts;

static bom_t bomA; /* Courant */
static bom_t bomV; /* Tension */

/* ----------- Tasks ----------- */

/* RT: calcule RMS A & V, applique seuil/TMS, pilote LEDs, envoie MMS */
static void task_protection(void *ctx)
{
    (void)ctx;
    static int last_state = -1; /* -1=unknown, 0=normal (vert), 1=trip (rouge) */

    double rmsA = 0.0, rmsV = 0.0;

#if SIMULATION
    /* Démo : valeurs fixes pour valider déclenchement + MMS rapidement */
    rmsA = 560.0;  /* au-dessus du seuil 550 par défaut -> déclenchement après TMS */
    rmsV = 240.0;  /* proche de 230 V nominal (informationnel) */
#else
    /* Mesure réelle côté HC-SR04 */
    int smp    = config_get_samples();
    int slp_us = config_get_sleep_ms() * 1000;
    rmsA = bea_rms_current_A(&bea, smp, slp_us);
    rmsV = bea_rms_voltage_V(&bea, smp, slp_us);
#endif

    /* Invalidité (ex: timeout capteur) */
    if (rmsA < 0 || rmsV < 0) {
        printf("[ERROR] Mesure invalide (A=%.2f, V=%.2f)\n", rmsA, rmsV);
        bom_set_invalid(&bomA, 1);
        bom_set_invalid(&bomV, 1);
        watchdog_kick(); /* évite FAULT inutile si capteur capricieux */
        return;
    } else {
        bom_set_invalid(&bomA, 0);
        bom_set_invalid(&bomV, 0);
    }

    

    /* Seuil + TMS sur chaque voie */
    int tripA = bom_check_with_tms(&bomA, rmsA);
    int tripV = bom_check_with_tms(&bomV, rmsV);
    // printf("[INFO] TRIPA et V =%d , %d\n",tripA,tripV);
    /* Logique de déclenchement : ANY (A OU V) */
    int trip = (tripA || tripV);
    // printf("[INFO] TRIP =%di",trip);
    if (trip) {
        bts_set_state(&bts, 1); /* LED rouge ON */
        if (last_state != 1) {
            // printf("ici0");
            time_t now = time(NULL);
            char ts[64];
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

            conf_add_log("TRIP_ON", "breaker -> RED (déclenchement)");
            printf("[INFO] RMS A=%.2f V=%.2f\n", rmsA, rmsV);
            printf("[ALERTE] Déclenchement! à %s\n.",ts);
            last_state = 1;
        }
        /* MMS-like : envoie la valeur A (format actuel "MMS: value=.. ts=..") */
        // printf("ici1");
        mms_send(rmsA);
        
    } else {
        bts_set_state(&bts, 0); /* LED verte ON */
        if (last_state != 0) {
            // printf("ici2");
            time_t now = time(NULL);
            char ts[64];
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
            conf_add_log("TRIP_OFF", "breaker -> GREEN (normal)");
            printf("[INFO] Retour à l'état normal à %s\n.",ts);
            last_state = 0;
        }
        // printf("ici3");
    }

    /* Heartbeat RT */
    watchdog_kick();
}

/* NRT: reload config demandé par MMS (POST /apply | /config) */
static void task_reload_config(void *ctx)
{
    (void)ctx;
    if (conf_need_reload()) {
        printf("[INFO] Reload config demandé par MMS.\n");
        if (config_load("config.json") != 0) {
            printf("[WARN] Reload config a échoué, maintien des valeurs actuelles.\n");
        } else {
            double thr_A   = config_get_threshold_A();
            int    tmsms_A = config_get_tms_A_ms();
            double thr_V   = config_get_threshold_V();
            int    tmsms_V = config_get_tms_V_ms();
            printf("[INFO] thr_A =%.2f",thr_A) ;
            bom_init(&bomA, thr_A, tmsms_A);
            bom_init(&bomV, thr_V, tmsms_V);

            char info[128];
            snprintf(info, sizeof(info), "APPLIED thr_A=%.3f tms_A=%d thr_V=%.3f tms_V=%d smp=%d slp=%d mode=%s",
                     thr_A, tmsms_A,thr_V, tmsms_V, config_get_samples(), config_get_sleep_ms(), config_get_mode());
            conf_add_log("CONFIG_APPLIED", info);
            printf("[INFO] Nouvelles valeurs: %s\n", info);
        }
        conf_clear_reload_flag();
    }
}

/* NRT: watchdog (détection retard RT) */
static void task_watchdog(void *ctx)
{
    (void)ctx;
    if (watchdog_check()) {
        printf("[FAULT] Watchdog: task_protection en retard > %d ms\n",
               watchdog_get_timeout_ms());
        /* Option: conf_add_log("FAULT","watchdog timeout"); */
    }
}

/* ----------- Entrée principale ----------- */

int main(void)
{
    printf("[INFO] Démarrage du système de protection.\n");

    /* Config initiale */
    if (config_load("config.json") != 0) {
        printf("[WARN] Fichier config absent ou invalide. Valeurs par défaut appliquées.\n");
    }
    double thr_A  = config_get_threshold_A();
    int    tms_A  = config_get_tms_A_ms();
    double thr_V  = config_get_threshold_V();
    int    tms_V  = config_get_tms_V_ms();
    int    smp  = config_get_samples();
    int    slp  = config_get_sleep_ms();
    printf("[INFO] Paramètres init: threshold_A=%.2f, TMS_A=%d ms,threshold_V=%.2f, TMS_V=%d ms, samples=%d, sleep=%d ms, mode=%s\n",
           thr_A, tms_A,thr_V, tms_V, smp, slp, config_get_mode());

    /* GPIO chip */
    struct gpiod_chip *chip = gpiod_chip_open(CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    /* Init BEA/BEL/BTS */
    if (bea_init(chip, &bea) < 0) return 1;
    if (bel_init(chip, &bel, 24, 1) < 0) return 1;   /* ex. bouton sur line 24, active-high */
    if (bts_init(chip, &bts) < 0) return 1;

    /* Init BOM A et V (seuil/TMS identiques en format historique) */
    bom_init(&bomA, thr_A, tms_A);
    bom_init(&bomV, thr_V, tms_V);

    /* MMS SCADA (multi-interfaces: 192.168.0.101 et 192.168.7.3) */
    if (conf_start(9090) != 0) {
        printf("[WARN] MMS HTTP non démarré.\n");
    }

    /* Watchdog (1500 ms par défaut) */
    watchdog_init(WATCHDOG_TIMEOUT_MS_DEFAULT);

    /* Scheduler APS */
    aps_task_t      tasks_buf[8];
    aps_scheduler_t sch;
    aps_init(&sch, tasks_buf, 8);

    /* RT: protection (100 ms) */
    aps_add_task(&sch, task_protection, NULL, 100, 0);
    /* NRT: reload config (500 ms) */
    aps_add_task(&sch, task_reload_config, NULL, 500, 0);
    /* NRT: watchdog check (500 ms) */
    aps_add_task(&sch, task_watchdog, NULL, 500, 0);

    printf("[INFO] Démarrage du scheduler...\n");
    aps_run(&sch);  /* boucle bloquante */

    /* Arrêt propre (si jamais aps_run retourne) */
    conf_stop();
    gpiod_chip_close(chip);
    return 0;
}
