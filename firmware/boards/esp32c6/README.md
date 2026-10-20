# Portage ESP32-C6

Non développé pour l'instant — le travail en cours se concentre sur l'ESP32-S3
(`firmware/include/board_config.h`, environnements `s3_*`).

Mêmes étapes que pour le C3 (voir `firmware/boards/esp32c3/README.md`), avec
en plus un point d'attention spécifique au C6 :

- Le C6 ajoute le WiFi 6 et le 802.15.4 (Zigbee/Thread), inutiles ici — rester
  en WiFi 2.4 GHz classique + BLE 5, pas besoin d'activer ces stacks
  additionnels dans `platformio.ini` (éviter d'alourdir le firmware pour
  rien).
- Vérifier le support NimBLE-Arduino / Arduino-ESP32 pour le C6 dans la
  version de `platform = espressif32` utilisée (support ajouté
  progressivement — fixer une version de plateforme connue pour fonctionner
  avant de se lancer, plutôt que de prendre `espressif32` sans version).
