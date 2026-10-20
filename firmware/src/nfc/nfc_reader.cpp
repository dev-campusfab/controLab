#include "nfc_reader.h"
#include "../../include/board_config.h"

#if HAS_NFC
#include <Wire.h>
#include <Adafruit_PN532.h>

namespace nfc_reader {

static Adafruit_PN532 nfc(PIN_NFC_IRQ, PIN_NFC_RESET);
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
    // Le bus I2C (Wire.begin) est initialisé une seule fois dans main.cpp,
    // avant l'appel à nfc_reader::begin(), car il peut être partagé avec
    // l'écran OLED sur les variantes qui en ont un.
    nfc.begin();
    nfc.SAMConfig();
}

bool poll(String& uidOut) {
    uint8_t uid[7];
    uint8_t uidLength;

    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, READ_TIMEOUT_MS)) {
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
