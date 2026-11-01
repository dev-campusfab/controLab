#include "nfc_reader.h"
#include "../../include/board_config.h"

#if HAS_NFC
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "../core/hw_config.h"

namespace nfc_reader {

static Adafruit_PN532* pn532 = nullptr;
static bool active = false;
static String lastUid;
static uint32_t lastSeenMs = 0;
static const uint32_t REBADGE_COOLDOWN_MS = 2000; // même carte reposée = pas de nouvel event avant ce délai
static const uint16_t READ_TIMEOUT_MS = 50;        // court pour rester quasi non-bloquant dans la boucle principale

static String uidToHex(const uint8_t* uid, uint8_t len) {
    String out;
    char buf[3];
    for (uint8_t i = 0; i < len; i++) {
        snprintf(buf, sizeof(buf), "%02X", uid[i]);
        out += buf;
    }
    return out;
}

void begin() {
    active = false;
    if (pn532 != nullptr) {
        delete pn532;
        pn532 = nullptr;
    }

    const hw_config::NfcConfig& cfg = hw_config::nfc();
    if (!cfg.enabled || cfg.interface != "i2c") {
        // Désactivé via hw-config, ou interface "spi" (pas encore pilotée
        // par ce firmware — voir docs/API_CONTRACT.md).
        return;
    }

    // IMPORTANT : ne PAS appeler Wire.begin() ici. Sur Arduino-ESP32 3.x (le
    // nouveau driver I2C "-ng"), un second Wire.begin() déclenché en interne
    // par Adafruit_PN532 (appelé sans arguments dans son propre begin()) ne
    // se contente pas d'un warning "Bus already started" : le bus reste dans
    // un état invalide et toutes les transactions suivantes échouent
    // (ESP_ERR_INVALID_STATE en boucle). Wire.setPins() mémorise les pins
    // sans démarrer le bus ; pn532->begin() juste après les utilisera.
    //
    // Limite connue : si le bus I2C est déjà démarré (ex. reconfiguration à
    // chaud suite à un RELOAD_CONFIG alors que le NFC tournait déjà avec
    // d'autres pins), ce nouveau setPins() peut ne pas être pris en compte
    // tant que le bus n'a pas été réellement redémarré.
    Wire.setPins(cfg.sda, cfg.scl);

    pn532 = new Adafruit_PN532(cfg.irq, cfg.reset);
    pn532->begin();
    pn532->SAMConfig();
    active = true;
}

bool poll(String& uidOut) {
    if (!active || pn532 == nullptr) {
        return false;
    }

    uint8_t uid[7];
    uint8_t uidLength;

    if (!pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, READ_TIMEOUT_MS)) {
        return false;
    }

    String hex = uidToHex(uid, uidLength);
    uint32_t now = millis();
    if (hex == lastUid && (now - lastSeenMs) < REBADGE_COOLDOWN_MS) {
        // Même carte laissée sur le lecteur : pas de nouvel événement tant
        // que le cooldown n'est pas écoulé.
        lastSeenMs = now;
        return false;
    }

    lastUid = hex;
    lastSeenMs = now;
    uidOut = hex;
    return true;
}

} // namespace nfc_reader

#else // !HAS_NFC

namespace nfc_reader {

void begin() {}

bool poll(String& uidOut) {
    (void)uidOut;
    return false;
}

} // namespace nfc_reader

#endif
