
// src/watchdog.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Timeout par défaut (ms) si rien n'est précisé lors de l'init
#define WATCHDOG_TIMEOUT_MS_DEFAULT 1500

// Initialise le watchdog avec un timeout (ms).
// Retourne 0 si OK, -1 si argument invalide.
int watchdog_init(int timeout_ms);

// "Kick" (heartbeat) à appeler depuis task_protection quand le cycle s'est bien déroulé.
void watchdog_kick(void);

// Vérifie si le délai depuis le dernier kick dépasse le timeout.
// Retourne 1 si FAULT détecté, 0 sinon.
int watchdog_check(void);

// Indique si le watchdog est actuellement en faute (dernière vérif).
int watchdog_is_fault(void);

// Récupère le timeout configuré (ms).
int watchdog_get_timeout_ms(void);

#ifdef __cplusplus
}
#endif
