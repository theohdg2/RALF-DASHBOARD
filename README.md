# R.A.L.F. : poste de pilotage sur GitHub Pages

## Fonctionnement

Le site est statique. Les données partagées sont enregistrées dans `data/project.json` avec l’API GitHub Contents. Chaque enregistrement produit un commit. Le site lit la dernière version sur GitHub, indépendamment des délais de publication GitHub Pages.

La consultation est publique. Pour modifier, un membre autorisé saisit son propre jeton personnel GitHub à accès fin dans « Connexion pour modifier ». Limiter le jeton au dépôt RALF-DASHBOARD avec `Contents: Read and write`. Le membre doit aussi avoir des droits d’écriture sur le dépôt. Aucun jeton ne doit être placé dans le code, config.js, les données, une URL ou un export. Le site garde le jeton en mémoire, jamais dans localStorage. Fermer ou recharger la page termine la session d’édition.

## Hébergement

1. Déposer le contenu de cette archive à la racine d’une branche dédiée, par exemple `main`. Conserver `index.html`, `app.js`, `style.css`, `sync.js`, `config.js`, `.nojekyll` et `data/project.json`.
2. Dans `config.js`, renseigner le propriétaire, le dépôt, la branche de données et le chemin JSON. La configuration actuelle vise `theohdg2/RALF-DASHBOARD`, branche `main`, chemin `data/project.json`.
3. Sur GitHub : Settings > Pages > Build and deployment > Deploy from a branch. Sélectionner la branche `main` et le dossier `/ (root)`. Enregistrer.
4. Ouvrir l’adresse fournie par GitHub Pages. La même interface fonctionne aussi sur un autre hébergeur statique.
5. Vérifier le voyant « Données partagées sur GitHub ». Un refus ou un fichier absent reste affiché comme une erreur, sans prétendre avoir sauvegardé.
6. Pour modifier, saisir le jeton dans la fenêtre de connexion, enregistrer un essai puis vérifier le nouveau commit de `data/project.json`. Recharger le site depuis un autre navigateur pour vérifier le résultat.

Les mises à jour du code passent par des commits des fichiers HTML/CSS/JS dans la branche publiée. Les mises à jour du suivi passent par l’API, sans reconstruire le site. GitHub Pages doit être activé dans les réglages du dépôt. Cette archive ne l’active pas à elle seule.

## Conflits et incidents

Avant une écriture, le site compare la révision distante. GitHub vérifie aussi le SHA au moment de l’enregistrement. Si un autre membre a modifié les données, le site refuse d’écraser sa version. Exporter le brouillon, recharger la version partagée, puis réappliquer les changements.

Un échec réseau ne produit pas de message de succès. Le brouillon est conservé sur cet appareil si le stockage local est disponible. À la session suivante, la fenêtre de connexion propose son export. Les autres visiteurs ne voient que les versions effectivement sauvegardées sur GitHub.

La lecture automatique s’actualise toutes les deux minutes quand la page est visible et sans formulaire actif. « Recharger la version partagée » force la lecture. Les limites de l’API GitHub restent applicables.

## Ancien suivi local

Les données déjà saisies dans la première version du site restent dans son navigateur d’origine. Avant de remplacer cette version, utiliser « Exporter le projet ». Puis se connecter en édition dans la nouvelle version et importer le JSON dans Rapport & PC2. L’import fusionne les enregistrements et remplace le profil de paramètres après sauvegarde.

## Validation

`node --test tests/sync.test.cjs`

Tests : Unicode, sauvegarde sur le transport GitHub, lecture par un second client, refus anonyme, conflits avant et pendant l’écriture, coupure réseau, refus de permissions. Les tests utilisent un transport simulé. L’enregistrement réel sur le dépôt exige la présence de `data/project.json` et un jeton autorisé.

## Données du projet

Les hauteurs 20 / 10 / 5 cm sont provisoires. La couleur du troisième scotch reste inconnue. Les campagnes historiques ne sont pas recalculées par les réglages du tableau de bord. Les données du dépôt public sont accessibles à tous.


## Variante ESP32-S3 N16R8

Le dossier [esp32](esp32/README.md) contient le firmware PlatformIO et le générateur du site local. Cette variante fonctionne sans Internet ni jeton GitHub, avec sauvegarde dans la mémoire de l’ESP32, comptes locaux par email, gestion admin et dépôt des rapports. Voir le guide pour le chargement USB, le Wi-Fi et la clé d’édition locale.

La version ESP32 comprend aussi la connexion mémorisée, les responsables et échéances du planning, les profils associés aux essais, le journal du suivi, les notes par point de contrôle et une archive du suivi avec les documents. Voir [le guide ESP32](esp32/README.md).

Les outils ESP32 proposent également Mes tâches, la recherche globale, les photos et problèmes liés aux essais, la comparaison de deux essais et les compteurs de stockage. Le bandeau inférieur et son ancien import ont été retirés.
