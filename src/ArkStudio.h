// src/ArkStudio.h
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Démarre le serveur HTTP (NRT) sur port donné (ex. 9090). Retour 0 si OK. */
int  conf_start(uint16_t port);
/* Arrête le serveur (join du thread). */
void conf_stop(void);

/* Flag global : le thread RT peut savoir qu'un reload est demandé (POST /apply|/config). */
int  conf_need_reload(void);
void conf_clear_reload_flag(void);

/* Ajoute une entrée dans le journal MMS (exposée via GET /logs). */
void conf_add_log(const char* action, const char* detail);

#ifdef __cplusplus
}
#endif
