# R.A.L.F. sur ESP32-S3 N16R8

Tableau de bord local, comptes membres, administration, planning et rapports. Le site fonctionne sur le Wi-Fi de l’ESP32, sans Internet. Les adresses email servent d’identifiants locaux. L’administrateur transmet les identifiants au membre, aucun service de messagerie n’est configuré.

Cible : ESP32-S3 avec 16 Mo de flash et 8 Mo de PSRAM octale, profil PlatformIO `esp32-s3-devkitc-1` adapté. Les ESP32 classiques de 4 Mo ne sont pas couverts par ce profil.

## Mise à jour d’une installation existante

Depuis le dossier `esp32`, fermer le moniteur série puis lancer :

```sh
git pull
pio run --target upload
pio run --target uploadfs
pio device monitor --baud 115200
```

Redémarrer la carte après les deux messages SUCCESS. Depuis la racine du dépôt, ajouter `-d esp32` aux commandes `pio`. Si le port change, le retrouver avec `pio device list`, puis ajouter `--upload-port COMx` au chargement.

Cette mise à jour conserve la table de partitions existante. Le projet et les essais restent dans la partition `state`. Le mot de passe du Wi-Fi existant reste conservé. Un nouveau système de comptes remplace la clé d’édition locale. Les anciennes interfaces doivent être remplacées avec `uploadfs` en même temps que le firmware.

## Premier accès administrateur

À la première initialisation des comptes, le moniteur série affiche :

```text
INITIAL ADMIN EMAIL: admin@ralf.local
INITIAL ADMIN PASSWORD: <mot de passe provisoire généré sur la carte>
```

Si ce message a été affiché avant l’ouverture du moniteur, envoyer la ligne suivante dans le moniteur série :

```text
RESET_ADMIN
```

Cette commande USB réinitialise le mot de passe du premier administrateur actif et affiche un nouveau mot de passe provisoire. Elle ferme ses sessions. Elle ne réinitialise ni le projet, ni les documents, ni les autres comptes.

Se connecter au Wi-Fi `RALF-…` indiqué dans le moniteur, puis ouvrir l’adresse affichée, normalement `http://192.168.4.1/`. Cliquer sur « Se connecter », saisir l’email et le mot de passe provisoire. Le site impose ensuite un mot de passe personnel de 12 à 128 caractères. « Mon compte » permet de remplacer `admin@ralf.local` par ton email et de modifier ton nom.

Les mots de passe initiaux ne sont pas réaffichés à chaque redémarrage. Après l’initialisation, la connexion quotidienne utilise l’email et le mot de passe du membre, sans clé d’édition.

## Membres et rôles

L’onglet « Gestion des membres » est accessible aux administrateurs. Il permet :

- d’ajouter un compte avec un nom et un email
- de choisir le rôle membre ou administrateur
- de modifier le nom, l’email, le rôle ou l’état actif
- de désactiver un compte en conservant son identité
- de générer un nouveau mot de passe provisoire

Chaque mot de passe provisoire est généré par l’ESP32 et affiché une fois à l’administrateur. Le membre le remplace à sa première connexion. Une réinitialisation de mot de passe ferme les sessions du compte. Le dernier administrateur actif ne peut pas être désactivé ou rétrogradé. La limite est de 12 comptes, comptes désactivés inclus.

Les membres connectés modifient le projet et déposent ou téléchargent les documents. Les visiteurs non connectés consultent le tableau de bord, sans modifier les données et sans accéder aux documents. Le serveur applique les droits, même pour une requête directe à l’API.

Les mots de passe sont enregistrés dans NVS sous forme PBKDF2-HMAC-SHA256, 60 000 itérations, avec un sel aléatoire individuel. Les mots de passe en clair ne sont pas conservés. Les sessions aléatoires restent en mémoire, expirent après huit heures et disparaissent au redémarrage. Le navigateur conserve le jeton de session dans le stockage de l’onglet pour retrouver la connexion après un rechargement. L’option « Rester connecté » utilise le stockage local et conserve la connexion après fermeture du navigateur, dans la limite des huit heures depuis la connexion. Le mot de passe et les droits du compte ne sont jamais mémorisés dans ce stockage. Chaque restauration vérifie le compte auprès de l’ESP32. Le redémarrage de la carte, la déconnexion, la désactivation du compte ou une réinitialisation de son mot de passe invalident la session. Cinq échecs de connexion déclenchent une pause de 30 secondes.

## Planning

Le planning trie les tâches par date de début, puis date de fin et titre. Les boutons de modification restent associés à la bonne tâche après le tri. Le même ordre s’applique à la liste des prochaines étapes sur l’accueil. Le tri est aussi disponible sur la version GitHub Pages.

## Documents des points de contrôle

L’onglet « Documents » permet de déposer et télécharger les rapports PC1, PC2, PC3, le rapport final et les autres documents. La date et le point de contrôle sont renseignés au dépôt. Les administrateurs peuvent supprimer un document après confirmation.

Formats acceptés : PDF, DOC/DOCX, PPTX, XLSX, PNG, JPG/JPEG, TXT et Markdown. Le nom est limité à 120 octets UTF-8. Les fichiers sont téléchargés comme pièces jointes. Ils ne sont pas exécutés sur l’ESP32.

Limites : 1 Mio par fichier, 24 documents, 2,5 Mio de documents au total. Le site affiche l’espace utilisé. Les documents sont conservés dans la partition `state`, séparée des fichiers web. Un transfert interrompu n’est pas ajouté à la liste des documents validés. Les fichiers temporaires non validés sont nettoyés au démarrage.

Exporter le JSON du projet n’exporte ni les comptes ni les documents. Les rapports se téléchargent individuellement depuis l’onglet Documents.

## Stockage et mise à jour du site

- `spiffs` : partition LittleFS web de 4 Mo, ciblée par `uploadfs`
- `state` : partition LittleFS distincte d’environ 3,9 Mo, projet et documents
- `nvs` : comptes, empreintes de mots de passe et identifiants Wi-Fi

Le projet utilise deux enregistrements alternés, avec génération et empreinte SHA-256. Au redémarrage, le firmware choisit la génération valide la plus récente. Le projet est limité à 64 Kio de JSON UTF-8 et 1 000 éléments par liste. Les conflits de modification entre deux appareils sont refusés.

Une mise à jour normale par `upload` puis `uploadfs` conserve ces données. Un effacement complet de flash ou une modification des partitions les supprimerait. Conserver la table fournie et exporter le suivi avant une intervention sur la mémoire.

Les fichiers web sont générés depuis les sources à la racine du dépôt. Le dépôt complet doit rester disponible. Les polices de repli fonctionnent sans Internet. La version GitHub Pages et l’ESP32 ont des stockages indépendants. Le transfert du suivi se fait par export/import JSON, sans synchronisation automatique.

## Wi-Fi existant

Copier `include/wifi_credentials.example.h` vers `include/wifi_credentials.h` et renseigner le SSID et le mot de passe du réseau personnel 2,4 GHz. Ce fichier est ignoré par Git. Recompiler et charger le firmware.

Après 12 secondes sans connexion, l’ESP32 démarre son point d’accès protégé. Le moniteur série indique son adresse. `http://ralf.local/` est annoncé par mDNS, selon la prise en charge du client. Le mode SSID/mot de passe ne gère pas eduroam/802.1X.

## Vérifications

```sh
node --test tests/native-fetch.test.cjs tests/planning-order.test.cjs esp32/tests/transport.test.cjs esp32/tests/accounts-client.test.cjs
python esp32/tests/assets_test.py
g++ -std=c++11 -Wall -Wextra -Werror esp32/tests/policy_test.cpp -o policy-test
./policy-test
pio run -d esp32
pio run -d esp32 --target buildfs
```

Les tests natifs portent sur les rôles, le dernier administrateur actif, l’expiration des sessions, les chemins et formats de fichiers et les dates. Les tests JavaScript vérifient les sessions, les messages d’erreur, le transfert multipart, les conflits du projet et les appels natifs à fetch. La vérification visuelle utilise une API locale de test. Les essais de stockage, de redémarrage et de transfert sur la carte physique restent à effectuer.

## API et robot

Principales routes : `/api/login`, `/api/logout`, `/api/me`, `/api/password`, `/api/profile`, `/api/users`, `/api/reset-password`, `/api/project`, `/api/documents`, `/api/document`, `/api/status`. Les données de compte et les documents exigent une session. Les opérations admin exigent le rôle administrateur.

Ce firmware héberge le tableau de bord. Il n’envoie pas d’ordres moteur ou servo au Pico. Les paramètres restent des profils de projet. Les valeurs provisoires et la couleur inconnue du troisième scotch sont conservées.
