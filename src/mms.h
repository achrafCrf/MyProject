
// src/mms.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MMS-like: envoi d'un message UDP multicast avec timestamp.
 */

#define MMS_GROUP "239.0.0.1" // Adresse multicast
#define MMS_PORT  5005        // Port UDP

/** Envoie un message MMS avec valeur et timestamp. */
int MMS_send(double value);

#ifdef __cplusplus
}
#endif
