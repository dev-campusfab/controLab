# ControLab — Protocole BLE d'appairage (provisioning)

Ce document est le **contrat exact** entre le firmware des modules et le
site web (via Web Bluetooth depuis le téléphone). Toute modification doit
être répercutée des deux côtés.

Le BLE **ne sert qu'au provisioning initial** (configurer WiFi + obtenir un
secret d'API). Une fois bound, toute la configuration "métier" (nom du
module, règles d'accès, etc.) passe par l'API REST HTTPS, pas par BLE.

## Découverte

- En mode SETUP, le module advertise en tant que périphérique BLE nommé :
  `ControLab-Setup-XXXXXX` où `XXXXXX` = les 6 derniers caractères hex de
  l'adresse MAC WiFi du module (sert d'identifiant visuel uniquement).
- L'UUID du service de setup est inclus dans les données d'advertising pour
  permettre un filtre précis côté `navigator.bluetooth.requestDevice`
  (`filters: [{ services: [SETUP_SERVICE_UUID] }]`).

## UUIDs

```
Service Setup          : 6c6f6c61-0001-4a3b-9c2e-5f3a1b2c0001
  Char DeviceInfo       : 6c6f6c61-0002-4a3b-9c2e-5f3a1b2c0001  (Read)
  Char WifiConfig       : 6c6f6c61-0003-4a3b-9c2e-5f3a1b2c0001  (Write)
  Char ServerConfig     : 6c6f6c61-0004-4a3b-9c2e-5f3a1b2c0001  (Write)
  Char Status           : 6c6f6c61-0005-4a3b-9c2e-5f3a1b2c0001  (Read, Notify)
  Char Command          : 6c6f6c61-0006-4a3b-9c2e-5f3a1b2c0001  (Write)
```

(Ces UUIDs sont définis dans `firmware/include/ble_protocol.h` côté
firmware — garder les deux fichiers synchronisés.)

Toutes les valeurs échangées sont des chaînes **JSON en UTF-8** (pas de
binaire packé), pour rester simples à manipuler en JS comme en C++.

## Caractéristique `DeviceInfo` (Read)

Lue par le site juste après connexion, pour identifier le module et savoir
quoi afficher/proposer.

```json
{
  "moduleId": "s3-7af2c1",
  "hwVariant": "s3_full",
  "fwVersion": "0.1.0",
  "capabilities": { "screen": true, "ledRing": true, "battery": true, "nfc": true },
  "bound": false
}
```

- `moduleId` : identifiant stable dérivé de l'adresse MAC (`s3-` + 6 hex),
  généré au premier boot, stocké en NVS, ne change jamais ensuite.
- `capabilities.nfc` : tous les modules ControLab n'ont pas forcément un
  lecteur NFC (certaines variantes n'en embarquent pas). Le site doit
  adapter l'interface de config du module en conséquence (pas d'écran de
  "règles d'accès par carte" si `nfc` est `false`), voir
  `WEBSITE_INSTRUCTIONS.md`.
- `bound` : si `true`, le module est déjà appairé à un compte site — dans ce
  cas le site ne doit PAS renvoyer de nouveau `ServerConfig` sans confirmation
  explicite de l'utilisateur (ré-appairage = perte de l'ancien secret côté
  module, il faudra re-générer un token depuis le site pour l'ancien module
  s'il doit être repris).

## Caractéristique `WifiConfig` (Write)

```json
{ "ssid": "Fablab-WiFi", "password": "xxxxxxxx" }
```

Le module stocke ces identifiants en NVS immédiatement. Il ne tente la
connexion WiFi qu'une fois `ServerConfig` également reçu (voir plus bas).

## Caractéristique `ServerConfig` (Write)

```json
{ "apiBaseUrl": "https://controlab.example.org/api", "bindToken": "a1b2c3..." }
```

- `bindToken` : jeton **à usage unique**, généré par le site au moment où
  l'utilisateur clique sur "Ajouter un nouveau module", valable un temps
  limité (recommandé : 5 minutes), scellé côté site au `moduleId` lu dans
  `DeviceInfo` avant écriture (le site doit vérifier `DeviceInfo.moduleId`
  correspond au module pour lequel le token a été émis — sécurité contre un
  mauvais appareil sélectionné dans le picker Bluetooth).

Dès réception de `ServerConfig`, le module :
1. Stocke `apiBaseUrl` en NVS.
2. Passe en état `CONNECTING`, tente le WiFi avec les creds reçus (timeout
   ~20s, 3 tentatives).
3. Si WiFi OK, appelle `POST {apiBaseUrl}/modules/bind` avec
   `{ "moduleId": "...", "bindToken": "..." }` (voir `API_CONTRACT.md`).
4. Si la réponse contient un `moduleSecret`, le stocke en NVS, marque
   `bound = true`, passe en `RUNNING`, et notifie `Status` = `BOUND_OK`.
5. En cas d'échec à n'importe quelle étape, notifie `Status` avec le code
   d'erreur correspondant et reste en SETUP pour permettre un nouvel essai.

## Caractéristique `Status` (Read + Notify)

```json
{ "state": "SETUP", "wifiConnected": false, "serverReachable": false, "error": null }
```

Valeurs possibles de `state` : `SETUP`, `CONNECTING`, `BINDING`, `BOUND_OK`,
`RUNNING`, `ERROR`.
Valeurs possibles de `error` : `null`, `WIFI_AUTH_FAILED`, `WIFI_TIMEOUT`,
`BIND_TOKEN_INVALID`, `BIND_TOKEN_EXPIRED`, `SERVER_UNREACHABLE`.

Le site doit s'abonner (`startNotifications`) à cette caractéristique
pendant toute la procédure pour afficher une progression à l'utilisateur
plutôt que de "polling" en lecture.

## Caractéristique `Command` (Write)

Chaîne simple (pas de JSON) :
- `"REBOOT"`
- `"FACTORY_RESET"` — efface toute la NVS (WiFi, secret, moduleId *recalculé*
  au reboot suivant puisque dérivé de la MAC, donc en pratique le
  `moduleId` ne change pas, mais `bound` repasse à `false`).
- `"ENTER_SETUP"` — ne fonctionne que si le module est déjà `RUNNING` et
  authentifié via API au préalable côté site (le site envoie d'abord un
  ordre `POST /modules/{id}/enter-setup` en REST, qui réveille le module en
  BLE pendant une fenêtre de 5 minutes ; voir `API_CONTRACT.md`). Cette
  caractéristique BLE `Command=ENTER_SETUP` n'est donc utilisable qu'une
  fois déjà en fenêtre de setup — elle sert surtout en debug local.

## Déroulé complet côté site (résumé, détaillé dans WEBSITE_INSTRUCTIONS.md)

1. Utilisateur clique "Ajouter un nouveau module".
2. Site appelle `navigator.bluetooth.requestDevice({ filters: [{ services: [SETUP_SERVICE_UUID] }] })`.
3. Connexion GATT, lecture `DeviceInfo`.
4. Site crée l'enregistrement module en base (statut `pending`), génère un
   `bindToken`.
5. Site écrit `WifiConfig` puis `ServerConfig`.
6. Site s'abonne à `Status`, affiche la progression jusqu'à `BOUND_OK` ou
   une erreur.
7. Une fois `BOUND_OK` : redirection vers la page de configuration complète
   du module (REST, plus de BLE nécessaire).
