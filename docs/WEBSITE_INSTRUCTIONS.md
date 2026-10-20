# Instructions pour l'agent/équipe en charge du site web ControLab

Ce document s'adresse à qui développe la **plateforme web** de ControLab
(backend + frontend). Le firmware des modules ESP32 est développé séparément
(dossier `firmware/` de ce même dépôt) et ne doit pas être modifié ici — en
revanche, les deux documents ci-dessous sont un **contrat figé** que le site
doit respecter à la lettre :

- `docs/BLE_PROTOCOL.md` — UUIDs, format JSON, séquence exacte de
  l'appairage Bluetooth LE entre le téléphone (navigateur) et un module.
- `docs/API_CONTRACT.md` — endpoints REST que le backend doit exposer pour
  que les modules déjà appairés fonctionnent (bind, heartbeat, scan).

Lis ces deux fichiers avant de commencer. Les instructions ci-dessous
décrivent le travail à faire côté site pour que le flux décrit dans ces
contrats fonctionne de bout en bout.

## 1. Contexte fonctionnel

ControLab identifie les adhérents d'un fablab via carte NFC sur des modules
physiques (boîtiers ESP32). Chaque module doit être "appairé" une fois
(provisioning WiFi + obtention d'un secret d'API) avant de pouvoir
fonctionner. L'appairage se fait **depuis le site, via le téléphone de
l'admin, en Bluetooth Low Energy (Web Bluetooth API)** — pas d'app mobile
dédiée.

Flux utilisateur recherché :
1. Un module neuf démarre tout seul en mode "setup" (il advertise en BLE).
2. L'admin ouvre le site sur son téléphone, va sur la page de gestion des
   modules, clique "Ajouter un nouveau module".
3. Le navigateur (via Web Bluetooth) scanne les appareils BLE à proximité,
   l'admin sélectionne le module dans la popup native du navigateur.
4. Le site discute en BLE avec le module pour lui transmettre le WiFi du
   fablab et un jeton d'appairage, pendant qu'il suit la progression en
   temps réel (connexion WiFi → appel au serveur → succès).
5. Une fois appairé, le module n'utilise plus que l'API REST
   (`docs/API_CONTRACT.md`) — toute la config "métier" (nom du module,
   emplacement, règles d'accès par carte/adhérent, etc.) se fait ensuite
   via des pages/formulaires classiques du site.

## 2. Pré-requis techniques pour la partie Web Bluetooth

- Web Bluetooth **ne fonctionne qu'en HTTPS** (ou `localhost` en dev) et
  n'est disponible que sur navigateurs Chromium (Chrome/Edge Android, pas
  Safari iOS — à mentionner clairement dans l'UI si l'admin est sur iPhone :
  prévoir un message d'avertissement explicite plutôt qu'un bouton qui ne
  fait rien).
- L'appel `navigator.bluetooth.requestDevice(...)` doit être déclenché par un
  geste utilisateur direct (clic sur le bouton "Ajouter un module"), sinon le
  navigateur le refuse.
- Toujours filtrer par le service UUID de setup (voir `BLE_PROTOCOL.md`),
  jamais par nom, pour éviter d'afficher des appareils BLE non liés au
  fablab dans la popup de sélection.

## 3. Ce qu'il faut construire — Frontend

### 3.1 Page "Modules"

- Liste des modules existants avec statut (bound/pending/offline — dérivé du
  dernier heartbeat reçu), variante matérielle, dernière activité.
- Bouton "Ajouter un nouveau module" déclenchant le flux BLE ci-dessous.
- Action "Reconfigurer le WiFi" sur un module déjà bound → appelle
  `POST /modules/{id}/enter-setup` côté backend, puis relance le même flux
  BLE de scan/connexion (le module aura rouvert sa fenêtre BLE après son
  prochain heartbeat, donc prévoir un délai/message d'attente côté UI,
  heartbeat envoyé toutes les ~60s par le module).

### 3.2 Assistant d'appairage (wizard pas-à-pas)

Implémenter une séquence d'écrans/état avec retour visuel à chaque étape
(spinner + libellé), idéalement adossée aux notifications de la
caractéristique `Status` (voir `BLE_PROTOCOL.md`) plutôt qu'à des timers
arbitraires :

1. **Recherche** : appel `requestDevice`, connexion GATT.
2. **Identification** : lecture `DeviceInfo`, affichage variante/moduleId.
   Si `bound == true`, avertir l'utilisateur ("ce module est déjà appairé à
   un autre compte, continuer l'écrasera") avant de poursuivre.
   Noter aussi `capabilities.nfc` : **certains modules ControLab n'ont pas de
   lecteur NFC** (ce n'est pas garanti sur toutes les variantes). Si
   `nfc == false`, la page de configuration du module qui suivra ne doit pas
   proposer d'écran "règles d'accès par carte" pour ce module — adapter le
   formulaire de config en fonction de cette capacité plutôt que de supposer
   qu'un module ControLab lit toujours des cartes.
3. **Saisie WiFi** : formulaire SSID/mot de passe (ou proposer un WiFi
   "fablab" pré-rempli/mémorisé côté site pour éviter de le ressaisir à
   chaque module).
4. **Envoi config** : le site appelle côté backend un endpoint qui crée le
   module (statut `pending`) et génère un `bindToken` **avant** d'écrire en
   BLE (voir section 4), puis écrit `WifiConfig` puis `ServerConfig`.
5. **Progression** : abonnement aux notifications `Status`, affichage de
   l'état (`CONNECTING`, `BINDING`, `RUNNING`...) et des erreurs
   (`WIFI_TIMEOUT`, `BIND_TOKEN_EXPIRED`, etc. — prévoir un message humain
   pour chaque code d'erreur listé dans `BLE_PROTOCOL.md`, avec un bouton
   "réessayer" qui réécrit simplement `ServerConfig` avec un nouveau token
   si besoin).
6. **Succès** : sur `BOUND_OK` (ou confirmation backend que le module est
   passé à un statut "actif"), redirection vers la page de configuration
   complète du module (nom, emplacement, règles d'accès...).

### 3.3 Détail technique Web Bluetooth (JS)

```js
const SETUP_SERVICE = "6c6f6c61-0001-4a3b-9c2e-5f3a1b2c0001";
const CHAR_DEVICE_INFO = "6c6f6c61-0002-4a3b-9c2e-5f3a1b2c0001";
const CHAR_WIFI_CONFIG = "6c6f6c61-0003-4a3b-9c2e-5f3a1b2c0001";
const CHAR_SERVER_CONFIG = "6c6f6c61-0004-4a3b-9c2e-5f3a1b2c0001";
const CHAR_STATUS = "6c6f6c61-0005-4a3b-9c2e-5f3a1b2c0001";
const CHAR_COMMAND = "6c6f6c61-0006-4a3b-9c2e-5f3a1b2c0001";

const device = await navigator.bluetooth.requestDevice({
  filters: [{ services: [SETUP_SERVICE] }],
});
const server = await device.gatt.connect();
const service = await server.getPrimaryService(SETUP_SERVICE);

const deviceInfoChar = await service.getCharacteristic(CHAR_DEVICE_INFO);
const deviceInfo = JSON.parse(
  new TextDecoder().decode((await deviceInfoChar.readValue()).buffer)
);

// ... créer le module côté backend avec deviceInfo.moduleId, récupérer bindToken ...

const statusChar = await service.getCharacteristic(CHAR_STATUS);
await statusChar.startNotifications();
statusChar.addEventListener("characteristicvaluechanged", (ev) => {
  const status = JSON.parse(new TextDecoder().decode(ev.target.value.buffer));
  // mettre à jour l'UI avec status.state / status.error
});

const encoder = new TextEncoder();
const wifiChar = await service.getCharacteristic(CHAR_WIFI_CONFIG);
await wifiChar.writeValue(encoder.encode(JSON.stringify({ ssid, password })));

const serverChar = await service.getCharacteristic(CHAR_SERVER_CONFIG);
await serverChar.writeValue(
  encoder.encode(JSON.stringify({ apiBaseUrl, bindToken }))
);
```

Points d'attention :
- `writeValue` a une taille max par paquet (MTU négocié, souvent ~180-512
  octets utiles) — les payloads JSON définis dans `BLE_PROTOCOL.md` restent
  petits, mais si un SSID/mot de passe est anormalement long, prévoir une
  validation côté formulaire plutôt que de laisser échouer silencieusement.
- Toujours fermer/catcher les erreurs de `device.gatt.connect()` (device
  hors de portée, refus utilisateur, timeout) avec un message clair.
- Garder une référence à `device` pour pouvoir appeler
  `device.gatt.disconnect()` proprement en cas d'abandon du wizard.

## 4. Ce qu'il faut construire — Backend

Implémenter les endpoints de `docs/API_CONTRACT.md`, plus la gestion module
côté admin :

- **Modèle de données module** : `moduleId` (reçu du firmware, immuable),
  `hwVariant`, `fwVersion`, `status` (`pending` / `active` / `offline`),
  `moduleSecret` (hashé en base), `lastSeenAt`, `batteryPercent`,
  `wifiRssi`, nom/emplacement (renseignés après le bind), et une table de
  règles d'accès (adhérent ↔ module, ou carte ↔ module selon le modèle
  métier retenu).
- **Génération de `bindToken`** : jeton aléatoire (ex. UUID v4 ou 128 bits
  aléatoires encodés hex), stocké côté serveur avec une expiration (5 min
  recommandé dans `BLE_PROTOCOL.md`), associé au `moduleId` annoncé par
  `DeviceInfo` **avant** d'être écrit en BLE — ne jamais générer un
  `bindToken` générique utilisable par n'importe quel module.
- **`POST /modules/bind`** : vérifie le token (existence, non expiré, bon
  `moduleId`), le consomme (usage unique), crée/active le module, génère et
  renvoie un `moduleSecret` fort (ex. 256 bits, base64url), stocké hashé.
- **`POST /modules/{id}/heartbeat`** : met à jour `lastSeenAt`,
  `wifiRssi`, `batteryPercent`, `fwVersion` ; renvoie une commande en
  attente si un admin a demandé un reboot/reset/ENTER_SETUP sur ce module
  depuis la dernière fois.
- **`POST /modules/{id}/scan`** : reçoit `cardUid`, doit résoudre
  rapidement (<300ms visé) la règle d'accès associée à cette carte pour ce
  module précis et renvoyer la décision (`granted`/`denied`/`unknown_card`),
  plus éventuellement un message/couleur/texte à afficher. Logger chaque
  scan (table d'historique) pour permettre un écran "derniers passages" côté
  admin.
- **`POST /modules/{id}/enter-setup`** : endpoint authentifié admin (pas
  appelé par le module) qui positionne un drapeau lu au prochain heartbeat.
- Authentification des 3 endpoints "module" (bind excepté) via header
  `Authorization: Bearer {moduleSecret}` — comparer en temps constant contre
  le hash stocké.
- Rate-limiting raisonnable par module sur `scan`/`heartbeat` (protection
  contre un module qui boucle en erreur).

## 5. Ce qui n'est PAS à faire ici

- Ne pas essayer de modifier ou documenter le firmware — c'est géré dans
  `firmware/`.
- Ne pas inventer de nouveaux endpoints/caractéristiques BLE sans mettre à
  jour `docs/BLE_PROTOCOL.md` et `docs/API_CONTRACT.md` en conséquence (et
  sans coordination avec la partie firmware, puisque les deux doivent rester
  synchronisés).
- La sémantique exacte des règles d'accès (qui a le droit d'utiliser quelle
  machine/porte) n'est pas définie ici — c'est un choix de modèle de données
  propre à la plateforme web, tant que l'API expose les 3 endpoints attendus
  par le module avec le format de réponse décrit.
