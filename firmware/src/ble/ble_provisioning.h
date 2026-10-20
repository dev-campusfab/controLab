#pragma once
#include <Arduino.h>

// Gère le service BLE de provisioning (NimBLE). Le binding réseau proprement
// dit (connexion WiFi + appel /modules/bind) est déclenché depuis la boucle
// principale dès qu'un drapeau "pendingBind" est levé par le callback BLE
// (on évite de faire du WiFi/HTTP bloquant dans un callback NimBLE).
namespace ble_provisioning {

void begin();
void stop();

// À appeler en boucle depuis main : exécute la connexion WiFi + le bind
// réseau si le site vient d'écrire ServerConfig, et notifie Status.
void loopTask();

bool isPendingBind();

struct PendingBindRequest {
    String apiBaseUrl;
    String bindToken;
};
PendingBindRequest consumePendingBind();

// Notifie la caractéristique Status avec l'état courant de core::ctx.
void notifyStatus();

} // namespace ble_provisioning
