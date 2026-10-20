#pragma once
#include <Arduino.h>

namespace core {

enum class State {
    BOOT,
    SETUP,       // advertising BLE, attend le provisioning
    CONNECTING,  // WiFi en cours de connexion
    BINDING,     // appel POST /modules/bind en cours
    RUNNING,     // opérationnel : NFC + heartbeat
    ERROR
};

enum class ErrorCode {
    NONE,
    WIFI_AUTH_FAILED,
    WIFI_TIMEOUT,
    BIND_TOKEN_INVALID,
    BIND_TOKEN_EXPIRED,
    SERVER_UNREACHABLE
};

// État global partagé par les différents modules (BLE, UI, API). Volontai-
// rement simple (pas de vrai event bus) vu la taille du firmware.
struct Context {
    State state = State::BOOT;
    ErrorCode error = ErrorCode::NONE;
    bool setupWindowOpen = false; // fenêtre BLE ouverte même si déjà bound
};

extern Context ctx;

const char* stateToString(State s);
const char* errorToString(ErrorCode e);

void setState(State s);
void setError(ErrorCode e);

// Permet à ble_provisioning de s'abonner aux changements d'état pour
// notifier la caractéristique Status sans dépendance circulaire.
void onStateChangeCallback(void (*cb)());

} // namespace core
