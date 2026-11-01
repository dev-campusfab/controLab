#pragma once
#include <Arduino.h>

// Configuration matérielle dynamique (NFC/LED), reçue depuis
// GET /modules/{moduleId}/hw-config (voir docs/API_CONTRACT.md) et
// persistée en NVS pour fonctionner hors-ligne entre deux synchronisations.
//
// Tant qu'aucune config n'a jamais été reçue du site, les valeurs
// compilées de board_config.h (HAS_NFC/PIN_NFC_*, HAS_LED_RING/PIN_LED_RING_*)
// servent de valeurs par défaut — le module reste utilisable avant le
// premier contact réseau.
namespace hw_config {

struct NfcConfig {
    bool enabled = false;
    String interface; // "i2c" (seule interface gérée actuellement) ou "spi"
    int sda = -1, scl = -1, irq = -1, reset = -1; // i2c ; -1 = non précisé
};

struct LedConfig {
    bool enabled = false;
    String model; // "ws2812" | "sk6812" | "ws2811"
    int pin = -1;
    int count = 0;
};

// Charge la config persistée en NVS, ou applique les valeurs compilées de
// board_config.h si rien n'a encore été synchronisé.
void begin();

const NfcConfig& nfc();
const LedConfig& led();

// Parse, valide et persiste en NVS un JSON reçu de GET /hw-config. Les pins
// invalides ou en conflit avec des broches réservées du board sont rejetées
// (la section correspondante retombe alors sur sa valeur par défaut) —
// jamais de pin appliquée telle quelle sans validation.
// Retourne true si au moins une section a été appliquée.
bool applyFromJson(const String& json);

} // namespace hw_config
