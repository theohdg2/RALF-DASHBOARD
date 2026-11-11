# R.A.L.F. sur ESP32-S3 N16R8

Cette variante héberge le tableau de bord et conserve les données sur l’ESP32. Elle fonctionne sans GitHub, sans jeton GitHub et sans Internet. Les appareils connectés au même ESP32 consultent le même projet.

Cible : ESP32-S3, 16 Mo de flash, 8 Mo de PSRAM octale, configuration PlatformIO `esp32-s3-devkitc-1`. Les identifiants USB et la procédure de démarrage dépendent de la carte portant le module. Cette version n’est pas destinée aux ESP32 classiques de 4 Mo.

## Charger sur la carte

Ouvrir le dossier `esp32` dans PlatformIO, puis brancher la carte par USB.

```sh
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor --baud 115200
```

Depuis la racine du dépôt, ajouter `-d esp32` à chaque commande `pio`.

Les ressources du site sont générées automatiquement depuis les fichiers à la racine du dépôt. Le dossier complet du dépôt doit donc rester disponible. `upload` charge le firmware et sa table de partitions. `uploadfs` charge les fichiers du site dans la partition web.

Au premier démarrage, le moniteur série affiche :

- le nom du Wi-Fi `RALF-…` et son mot de passe aléatoire
- l’adresse du site, normalement `http://192.168.4.1/` en mode point d’accès
- la clé d’édition locale, différente du mot de passe Wi-Fi

Connecter le téléphone ou le PC à ce Wi-Fi, ouvrir l’adresse affichée, puis cliquer sur « Connexion pour modifier ». Saisir la clé d’édition ESP32 et cliquer sur « Activer l’édition ». Aucun jeton GitHub n’est utilisé. Si le téléphone signale un Wi-Fi sans Internet, garder la connexion pour accéder au site local.

La clé et le mot de passe sont générés au premier démarrage et conservés dans NVS. Le redémarrage les réaffiche au port série. La consultation est accessible aux appareils du réseau. L’écriture exige la clé locale. Les exports JSON et CSV sont téléchargés sur le téléphone ou le PC.

## Utiliser un Wi-Fi existant

Copier `include/wifi_credentials.example.h` vers `include/wifi_credentials.h`. Renseigner le SSID et le mot de passe du réseau personnel 2,4 GHz. Ce fichier est ignoré par Git. Recompiler et charger le firmware.

L’ESP32 tente la connexion pendant 12 secondes. Sans connexion, il démarre son propre point d’accès protégé. Le mode SSID/mot de passe ne gère pas eduroam/802.1X. L’adresse IP est indiquée au moniteur série. `http://ralf.local/` est aussi annoncé par mDNS, selon la prise en charge de l’appareil client.

## Sauvegarde

- `spiffs` : partition LittleFS des fichiers web, 4 Mo
- `state` : partition LittleFS distincte des données, environ 3,9 Mo
- `nvs` : clé d’édition et mot de passe du point d’accès

Chaque validation d’un formulaire enregistre le projet. Le firmware utilise deux enregistrements alternés avec génération et empreinte SHA-256. Il vérifie le nouvel enregistrement avant de le rendre actif. Au redémarrage, il sélectionne la génération valide la plus récente. En cas d’interruption pendant une sauvegarde, le dernier enregistrement validé reste la référence.

La mémoire des données est formatée automatiquement uniquement si toute sa partition est vierge. Une partition non vierge illisible n’est pas effacée automatiquement. Le journal série permet de diagnostiquer ce cas.

Les mises à jour du site par `uploadfs` ne touchent pas la partition `state`. Une modification de la table de partitions, un effacement complet de la flash ou un outil de chargement configuré pour tout effacer supprimeraient cette séparation. Exporter le suivi avant ces opérations. Conserver la table fournie lors des mises à jour normales.

Le projet est limité à 64 Kio de JSON UTF-8 et 1 000 éléments par liste. Au-delà, le serveur refuse la sauvegarde. Pour un suivi long, exporter et archiver des essais. Les deux enregistrements sont une protection locale contre une sauvegarde interrompue, pas un historique Git et pas une sauvegarde externe.

Les conflits sont détectés avec la révision du projet. Si un autre membre a sauvegardé avant toi, ta version n’écrase pas la sienne. Exporter le brouillon, recharger le projet puis réappliquer les changements.

Le site GitHub Pages et l’ESP32 ont chacun leur copie du projet. Ils ne se synchronisent pas automatiquement. Pour transférer les données, exporter le JSON depuis une version et l’importer dans l’autre en session d’édition.

## API locale

| Route | Fonction |
| --- | --- |
| GET `/api/project` | Projet et révision SHA-256 |
| POST `/api/session` | Vérification de la clé locale |
| PUT `/api/project` | Validation et sauvegarde du projet |
| GET `/api/status` | État du stockage, génération et mémoire disponible |

L’écriture utilise `Authorization: Bearer <clé locale>` et `If-Match: "<révision>"`. Un conflit renvoie 409. Une mauvaise clé renvoie 401. La clé est gardée en mémoire dans la page, sans stockage dans le navigateur. Aucun accès externe n’est nécessaire pour charger le site, polices comprises.

## Vérifications

```sh
node --test esp32/tests/transport.test.cjs
python esp32/tests/assets_test.py
pio run -d esp32
pio run -d esp32 --target buildfs
```

Les tests de transport utilisent un serveur HTTP local de test. Ils vérifient deux clients, les conflits, l’authentification, l’encodage, la taille et les erreurs de stockage. Les tests des ressources vérifient les scripts générés, le gzip, le fonctionnement sans dépendance Internet et les partitions. Les essais réels de redémarrage, de coupure d’alimentation et de connexion Wi-Fi restent à effectuer sur la carte.

## Périmètre robot

Ce firmware héberge le tableau de bord. Les paramètres sont des profils de projet. Il n’envoie aucun ordre moteur ou servo et n’installe pas la logique du Pico. La liaison UART Pico/ESP32 et la transmission des réglages au robot sont une étape distincte. Les valeurs provisoires et la couleur inconnue du troisième scotch restent inchangées.
