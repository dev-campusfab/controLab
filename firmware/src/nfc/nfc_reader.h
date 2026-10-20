#pragma once
#include <Arduino.h>

namespace nfc_reader {

void begin();

// Non bloquant : renvoie true si une nouvelle carte vient d'être détectée.
// uidOut contient l'UID en hexadécimal majuscule (ex: "04A224B2C15E80").
// Anti-rebond interne : une même carte laissée sur le lecteur ne redéclenche
// pas tant qu'elle n'a pas été retirée (évite de spammer l'API).
bool poll(String& uidOut);

} // namespace nfc_reader
