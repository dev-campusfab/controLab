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

} // namespace ui_feedback
