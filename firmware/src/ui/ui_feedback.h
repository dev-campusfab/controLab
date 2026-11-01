#pragma once
#include <Arduino.h>
#include "../core/state_machine.h"

// Abstraction d'affichage : toutes les fonctions sont no-op si la variante
// matérielle n'a pas la capacité correspondante (voir board_config.h).
// Le reste du firmware appelle ces fonctions sans jamais tester HAS_SCREEN /
// HAS_LED_RING lui-même.
namespace ui_feedback {

void begin();

// Appelé à chaque changement d'état (SETUP, CONNECTING, RUNNING, ERROR...)
void showState(core::State state, core::ErrorCode error);

// Appelé après une réponse de /modules/{id}/scan
void showScanResult(const String& result, const String& message,
                    const String& ledColorHex, const String& displayText, bool beep);

// Boucle d'animation (pulse, clignotement) à appeler fréquemment depuis loop()
void tick();

// Appelé pendant l'appui long sur le bouton BOOT (factory reset), progress
// de 0.0 (vient de commencer) à 1.0 (seuil atteint). Donne un retour visuel
// pour que l'utilisateur sache que l'appui est pris en compte.
void showFactoryResetHold(float progress);

// Message affiché brièvement au démarrage (avant d'entrer dans l'état
// SETUP/CONNECTING/RUNNING normal) pour confirmer que le boot s'est bien
// passé. N'attend pas elle-même : c'est à l'appelant de laisser le message
// affiché le temps voulu (voir main.cpp).
void showBootOk();

// Reconstruit l'anneau de LEDs à partir de hw_config::led() (pins/nombre
// peuvent changer après un RELOAD_CONFIG). No-op si la variante n'a pas
// d'anneau de LEDs.
void reloadLedConfig();

} // namespace ui_feedback
