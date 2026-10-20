# Portage ESP32-C3

Non développé pour l'instant — le travail en cours se concentre sur l'ESP32-S3
(`firmware/include/board_config.h`, environnements `s3_*`).

Pour porter sur C3 (mono-cœur, moins de RAM, pas de PSRAM) :

1. Ajouter un bloc `#elif defined(VARIANT_C3_xxx)` dans
   `firmware/include/board_config.h` avec les pins réelles du boîtier C3
   (`PIN_I2C_SDA`/`PIN_I2C_SCL` pour le bus I2C partagé PN532 + écran
   éventuel, `PIN_NFC_IRQ`/`PIN_NFC_RESET` pour le PN532 — le C3 a moins de
   GPIO disponibles, vérifier qu'il en reste assez une fois l'UART/USB-JTAG
   pris en compte).
2. Décommenter/adapter l'environnement `[env:c3_compact]` dans
   `firmware/platformio.ini` (board `esp32-c3-devkitm-1` ou le board exact
   utilisé).
3. Vérifier que NimBLE-Arduino, Adafruit PN532 et ArduinoJson tournent
   correctement sur C3 (NimBLE : oui, supporté officiellement). Le C3 n'a
   qu'un seul cœur donc attention à la latence du stack BLE qui partage le
   CPU avec la boucle principale — augmenter `scanPollIntervalMs` par défaut
   si besoin.
4. Aucun changement de logique métier n'est nécessaire : tout le code dans
   `firmware/src/` est générique (il ne lit que les macros de
   `board_config.h`).
