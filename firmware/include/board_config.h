#pragma once
// Définitions matérielles par variante. Un seul fichier source, le flag de
// build (-D VARIANT_xxx, défini dans platformio.ini) sélectionne la bonne
// section. Ajouter une nouvelle variante = ajouter un bloc #elif ici + un
// [env:] dans platformio.ini, sans toucher au reste du firmware.

#include <Arduino.h>

// --- ESP32-S3 : variante FULL ---------------------------------------------
// Pinout réel du devboard utilisé actuellement pour le dev/test :
// Waveshare ESP32-S3-RLCD-4.2 (écran réflectif ST7305 400x300, SPI).
// NFC et anneau de LEDs désactivés pour l'instant : rien n'est câblé sur ce
// prototype, on fait d'abord tourner le board lui-même (écran + BLE/WiFi).
// À réactiver (HAS_NFC / HAS_LED_RING) une fois le câblage fait, sur ce
// board ou sur un boîtier dédié.
#if defined(VARIANT_S3_FULL)
  #define BOARD_VARIANT_NAME   "s3_full"
  #define HAS_SCREEN           1
  #define HAS_LED_RING         0
  #define HAS_BATTERY_MON      1
  #define HAS_NFC              0

  // Écran ST7305 en SPI matériel (pinout officiel Waveshare RLCD-4.2)
  #define PIN_SCREEN_SCK       11
  #define PIN_SCREEN_MOSI      12
  #define PIN_SCREEN_DC        5
  #define PIN_SCREEN_CS        40
  #define PIN_SCREEN_RESET     41

  // Bus I2C du board (SHTC3 + codecs audio) — pas utilisé par le firmware
  // ControLab pour l'instant, gardé en référence si on y ajoute le PN532.
  #define PIN_I2C_SDA          13
  #define PIN_I2C_SCL          14

  #define PIN_BATTERY_ADC      4
  #define PIN_SETUP_BUTTON     0   // bouton BOOT

// --- ESP32-S3 : variante COMPACT (pas d'écran, une LED statut) -----------
#elif defined(VARIANT_S3_COMPACT)
  #define BOARD_VARIANT_NAME   "s3_compact"
  #define HAS_SCREEN           0
  #define HAS_LED_RING         0
  #define HAS_BATTERY_MON      0
  #define HAS_STATUS_LED       1
  #define HAS_NFC              1

  #define PIN_I2C_SDA          9
  #define PIN_I2C_SCL          10
  #define PIN_NFC_IRQ          11
  #define PIN_NFC_RESET        12

  #define PIN_STATUS_LED       8
  #define PIN_SETUP_BUTTON     0

// --- ESP32-S3 : variante NOSCREEN (anneau LEDs + batterie, pas d'écran) --
#elif defined(VARIANT_S3_NOSCREEN)
  #define BOARD_VARIANT_NAME   "s3_noscreen"
  #define HAS_SCREEN           0
  #define HAS_LED_RING         1
  #define HAS_BATTERY_MON      1
  #define HAS_NFC              1
  #define LED_RING_COUNT       8

  #define PIN_I2C_SDA          9
  #define PIN_I2C_SCL          10
  #define PIN_NFC_IRQ          11
  #define PIN_NFC_RESET        12

  #define PIN_LED_RING_DATA    21
  #define PIN_BATTERY_ADC      4
  #define PIN_SETUP_BUTTON     0

#else
  #error "Aucune VARIANT_xxx definie : choisir un environnement PlatformIO (s3_full, s3_compact, s3_noscreen, ...)"
#endif

// Valeurs par défaut si une variante ne définit pas une macro optionnelle
#ifndef HAS_SCREEN
  #define HAS_SCREEN 0
#endif
#ifndef HAS_LED_RING
  #define HAS_LED_RING 0
#endif
#ifndef HAS_STATUS_LED
  #define HAS_STATUS_LED 0
#endif
#ifndef HAS_BATTERY_MON
  #define HAS_BATTERY_MON 0
#endif
#ifndef HAS_NFC
  // Toutes les variantes ControLab n'ont pas forcément un lecteur NFC
  // (certains modules n'en ont pas) : absent par défaut, à activer
  // explicitement dans le bloc de la variante qui en a un.
  #define HAS_NFC 0
#endif
