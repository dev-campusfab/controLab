#include "ui_feedback.h"
#include "../../include/board_config.h"

#if HAS_LED_RING
  #include <Adafruit_NeoPixel.h>
#endif

#if HAS_SCREEN
  #include <U8g2lib.h>
  #include <SPI.h>
#endif

namespace ui_feedback {

#if HAS_LED_RING
static Adafruit_NeoPixel ring(LED_RING_COUNT, PIN_LED_RING_DATA, NEO_GRB + NEO_KHZ800);
#endif

#if HAS_SCREEN
// Écran réflectif ST7305 400x300 (Waveshare ESP32-S3-RLCD-4.2), SPI matériel.
// Nécessite U8g2 >= 2.36.19 (support ST7305 ajouté à cette version).
static U8G2_ST7305_300X400_F_4W_HW_SPI screen(U8G2_R0, PIN_SCREEN_CS, PIN_SCREEN_DC, PIN_SCREEN_RESET);
#endif

static uint32_t lastTickMs = 0;
static bool blinkOn = false;
static core::State currentState = core::State::BOOT;

#if HAS_LED_RING
static uint32_t colorForState(core::State s) {
    switch (s) {
        case core::State::SETUP: return ring.Color(0, 60, 255);      // bleu
        case core::State::CONNECTING: return ring.Color(255, 160, 0); // orange
        case core::State::BINDING: return ring.Color(255, 160, 0);
        case core::State::RUNNING: return ring.Color(0, 90, 0);      // vert faible (idle)
        case core::State::ERROR: return ring.Color(200, 0, 0);       // rouge
        default: return ring.Color(40, 40, 40);
    }
}

static uint32_t hexToColor(const String& hex) {
    // "#RRGGBB" -> couleur NeoPixel. Retombe sur blanc si format invalide.
    if (hex.length() != 7 || hex[0] != '#') return ring.Color(255, 255, 255);
    long rgb = strtol(hex.c_str() + 1, nullptr, 16);
    return ring.Color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

static void fill(uint32_t color) {
    for (int i = 0; i < LED_RING_COUNT; i++) ring.setPixelColor(i, color);
    ring.show();
}
#endif

void begin() {
#if HAS_LED_RING
    ring.begin();
    ring.setBrightness(80);
    ring.clear();
    ring.show();
#endif
#if HAS_STATUS_LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#endif
#if HAS_SCREEN
    // SPI matériel avec les pins réelles du board (pas de MISO, l'écran est
    // en écriture seule) : à initialiser avant screen.begin(), qui réutilise
    // l'objet SPI global déjà configuré.
    SPI.begin(PIN_SCREEN_SCK, -1, PIN_SCREEN_MOSI, PIN_SCREEN_CS);
    screen.begin();
    screen.setFont(u8g2_font_6x10_tf);
#endif
}

static void renderScreenState(core::State s, core::ErrorCode e) {
#if HAS_SCREEN
    screen.clearBuffer();
    screen.setCursor(0, 12);
    screen.print("ControLab");
    screen.setCursor(0, 28);
    screen.print(core::stateToString(s));
    if (s == core::State::ERROR) {
        screen.setCursor(0, 44);
        screen.print(core::errorToString(e));
    }
    screen.sendBuffer();
#else
    (void)s; (void)e;
#endif
}

void showState(core::State state, core::ErrorCode error) {
    currentState = state;
    renderScreenState(state, error);
#if HAS_LED_RING
    fill(colorForState(state));
#elif HAS_STATUS_LED
    digitalWrite(PIN_STATUS_LED, state == core::State::RUNNING ? HIGH : LOW);
#endif
}

void showScanResult(const String& result, const String& message,
                    const String& ledColorHex, const String& displayText, bool beep) {
    (void)beep; // pas de buzzer dans les variantes actuelles

#if HAS_SCREEN
    screen.clearBuffer();
    screen.setCursor(0, 20);
    if (displayText.length() > 0) {
        screen.print(displayText);
    } else {
        screen.print(message);
    }
    screen.sendBuffer();
#else
    (void)message; (void)displayText;
#endif

#if HAS_LED_RING
    uint32_t color;
    if (ledColorHex.length() > 0) {
        color = hexToColor(ledColorHex);
    } else if (result == "granted") {
        color = ring.Color(0, 200, 0);
    } else {
        color = ring.Color(200, 0, 0);
    }
    fill(color);
#elif HAS_STATUS_LED
    (void)ledColorHex;
    digitalWrite(PIN_STATUS_LED, result == "granted" ? HIGH : LOW);
#else
    (void)result; (void)ledColorHex;
#endif
}

void showFactoryResetHold(float progress) {
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;

#if HAS_SCREEN
    screen.clearBuffer();
    screen.setCursor(0, 12);
    screen.print("Reset usine...");
    screen.setCursor(0, 28);
    screen.print("Relacher pour annuler");
    screen.setCursor(0, 44);
    screen.print(String((int)(progress * 100)) + "%");
    screen.sendBuffer();
#endif

#if HAS_LED_RING
    // Anneau qui se remplit de rouge au fur et à mesure de l'appui.
    int litCount = (int)(progress * LED_RING_COUNT);
    for (int i = 0; i < LED_RING_COUNT; i++) {
        ring.setPixelColor(i, i < litCount ? ring.Color(200, 0, 0) : ring.Color(0, 0, 0));
    }
    ring.show();
#elif HAS_STATUS_LED
    // Clignotement qui accélère à mesure qu'on approche du seuil.
    uint32_t now = millis();
    uint32_t period = 400 - (uint32_t)(progress * 350);
    digitalWrite(PIN_STATUS_LED, (now / (period / 2 + 1)) % 2 == 0 ? HIGH : LOW);
#endif
}

void tick() {
    uint32_t now = millis();
    if (now - lastTickMs < 400) return;
    lastTickMs = now;
    blinkOn = !blinkOn;

#if HAS_STATUS_LED
    if (currentState == core::State::SETUP || currentState == core::State::ERROR) {
        digitalWrite(PIN_STATUS_LED, blinkOn ? HIGH : LOW);
    }
#elif HAS_LED_RING
    if (currentState == core::State::SETUP || currentState == core::State::CONNECTING ||
        currentState == core::State::BINDING) {
        fill(blinkOn ? colorForState(currentState) : ring.Color(0, 0, 0));
    }
#endif
}

} // namespace ui_feedback
