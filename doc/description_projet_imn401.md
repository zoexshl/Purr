# IMN401 - Document de description du projet

## Informations générales

- **Cours**: IMN401
- **Session**: Hiver 2026
- **Équipe**: `E__` (a compléter)
- **Nom du projet**: `Purr` (éditeur/scène 3D temps réel)
- **Version du document**: `v1`
- **Date**: `2026-04-16`

> Note: ce document suit la structure demandée dans `ProjetSession.pdf` (sections 3.1 à 3.8).  
> Remplacer les zones `TODO` avant export en PDF.

---

## 3.1 Sommaire (1 page)

`Purr` est une application C++/OpenGL orientée infographie interactive permettant de construire et éditer des scènes 3D, d'importer des modèles, de gérer des transformations, et de visualiser le rendu avec différents paramètres de caméra, matériaux et éclairage.

Le projet est organisé autour d'un moteur (`Purr`) et d'une application cliente (`Sandbox`) :

- Le moteur fournit les couches de base: fenêtre, boucle d'application, événements, rendu, shaders, buffers, caméra, textures et interface ImGui.
- L'application `Sandbox` implémente l'éditeur de scène et les fonctionnalités visibles par l'utilisateur (hiérarchie, sélection, gizmos, import de modèles, animation de base, etc.).

Objectifs principaux du projet:

- Offrir un environnement d'édition de scène interactif.
- Démontrer des critères fonctionnels du cours en géométrie, transformation, caméra, texture et illumination.
- Produire une application compilable/reproductible sur Windows avec dépendances intégrées.

---

## 3.2 Interactivité (1 page)

L'application est conçue autour d'une boucle d'événements temps réel et d'une interface utilisateur ImGui.

### Entrées principales

- Clavier et souris pour la navigation/caméra.
- Sélection d'objets dans la hiérarchie.
- Contrôles d'édition (translation, rotation, échelle) via gizmos.
- Actions d'import de modèles (OBJ/FBX via chargeurs dédiés).
- Paramètres de matériaux/lumière/projection dans l'interface.

### Sorties principales

- Rendu 3D temps réel dans un framebuffer.
- Panneaux UI (hiérarchie, propriétés, commandes d'édition).
- Rétroaction visuelle de la sélection et de l'état des objets.
- Journaux d'exécution (logs) pour diagnostic (chargement modèles/textures, erreurs, avertissements).

### Formes d'interactivité observées

- Gestion d'une hiérarchie d'objets avec sélection simple et multiple (`Ctrl`).
- Renommage d'objets et opérations de parentage/groupement.
- Gizmos avec modes translation/rotation/échelle.
- Caméra orbite + zoom et modes de projection perspective/orthographique.
- Contrôles de lecture/arrêt pour éléments animés de la scène (selon objets configurés).

---

## 3.3 Technologie (1/2 page a 1 page)

### Langages et bibliothèques

- **C++17** pour le moteur et l'application.
- **OpenGL** pour le rendu.
- **GLSL** pour les shaders.
- **GLFW** pour la fenêtre et les entrées.
- **Glad** pour le chargement des fonctions OpenGL.
- **ImGui** pour l'interface utilisateur en mode immédiat.
- **GLM** pour les mathématiques (vecteurs, matrices, transformations).
- **Assimp** pour l'import de modèles 3D (FBX/OBJ/autres).
- **stb_image** pour le chargement de textures.
- **spdlog** pour la journalisation.
- **Premake5** pour la génération du projet Visual Studio.

### Outils de développement

- Visual Studio 2022 (toolset MSVC v143).
- Configuration multi-profils: `Debug`, `Release`, `Dist`.

### Justification technologique (a personnaliser)

- OpenGL + C++ permet un contrôle fin du pipeline graphique.
- ImGui accélère la création d'outils d'édition/debug.
- Assimp simplifie la prise en charge de plusieurs formats de modèles.

---

## 3.4 Compilation (1/2 page a 1 page)

### Prérequis

- Windows 10/11 x64
- Visual Studio 2022 (C++ Desktop Development)
- Python (si requis pour premake dans votre environnement)

### Procedure recommandée

1. Ouvrir une console a la racine du projet.
2. Generer les fichiers Visual Studio avec Premake (commande habituelle de l'equipe, ex. `premake5 vs2022`).
3. Ouvrir la solution generee.
4. Choisir la configuration `Debug` (ou `Release`) en `x64`.
5. Compiler la solution.
6. Lancer le projet de demarrage `Sandbox`.

### Dependances et DLL

- Les bibliotheques tierces sont inclues dans `Purr/vendor`.
- Le script `premake5.lua` configure le lien statique/dynamique et copie les DLL Assimp post-build vers le dossier de sortie de `Sandbox`.

### Verification minimale

- L'application demarre sans crash.
- La fenetre de rendu apparait.
- Les textures/modeles de demonstration se chargent.

---

## 3.5 Architecture (1 page)

Architecture en deux couches:

1. **Moteur (`Purr`)**
  - Application/loop (`Application`)
  - Gestion fenetre/evenements (`Window`, `WindowsWindow`, `Input`)
  - Systeme de couches (`Layer`, `LayerStack`)
  - Rendu (`RenderCommand`, `VertexArray`, `Buffer`, `Shader`, `Texture`, `Framebuffer`, `Camera`)
  - Chargement d'actifs (`ObjLoader`, `AssimpLoader`, `TextureManager`)
  - Interface (`ImGuiLayer`)
2. **Application (`Sandbox`)**
  - Couche principale d'edition et de rendu (`ExampleLayer`)
  - Structure de scene (`SceneObject`, materiaux, animations simples)
  - Outils d'edition (hierarchie, selection multiple, gizmos, import)

### Diagramme textuel simplifie

`Application` -> `LayerStack` -> `ExampleLayer` -> `Renderer/Camera/Shaders` -> OpenGL

`ExampleLayer` -> `ObjLoader/AssimpLoader` -> Meshes/Textures -> SceneObjects -> Draw loop

### Flux d'execution

1. Initialisation fenetre + contexte OpenGL.
2. Initialisation des ressources de rendu (shaders, meshes, framebuffer).
3. Boucle principale:
  - mise a jour logique (`OnUpdate`)
  - rendu de scene
  - rendu UI ImGui
4. Traitement des evenements (fermeture, input, etc.).

---

## 3.6 Fonctionnalites (criteres fonctionnels)

> IMPORTANT: valider les points ci-dessous avec toute l'equipe avant la remise.  
> L'evaluateur accorde la note selon la qualite de demonstration et la robustesse.

### Criteres implementes (verification code)

1. **Modeles 3D importes (Geometrie)**
  - Import de modeles via OBJ et Assimp (incluant FBX), avec prise en charge multi-maillage.
  - Association de textures materiaux lors de l'import, avec resolution de chemins.
  - Demonstration: importer un OBJ puis un FBX (ou dossier FBX) depuis le panneau Scene.
2. **Graphe de scene (Transformation)**
  - Les objets sont organises avec relation parent/enfant (`ParentIndex`).
  - Le rendu utilise les transformations monde via composition hierarchique.
  - Demonstration: parentage d'objets + deplacement du parent qui entraine les enfants.
3. **Selection multiple (Transformation)**
  - Selection multiple dans la hierarchie avec `Ctrl`.
  - Operations de groupe (ex. "Grouper sous pivot"), copie/suppression de selection.
  - Demonstration: selection de plusieurs objets, modification collective.
4. **Transformations interactives (Transformation)**
  - Gizmos interactifs en modes translation/rotation/echelle.
  - Raccourcis `T`, `R`, `S`; edition directe dans la vue.
  - Demonstration: manipuler un objet sur les 3 modes.
5. **Historique de transformation Undo/Redo (Transformation)**
  - Historique avec piles `Undo`/`Redo` (limite de profondeur), boutons UI et raccourcis `Ctrl+Z` / `Ctrl+Y`.
  - Les etats de scene sont captures/restaures pour revenir en arriere et reappliquer.
  - Demonstration: faire 3 transformations successives, annuler puis refaire.
6. **Animation classique par keyframes (Transformation)**
  - Systeme de keyframes TRS par objet (position, rotation, echelle, temps).
  - Interpolation temporelle, lecture/pause/stop, loop, vitesse et timeline.
  - Edition interactive des keyframes (ajout/suppression/modification) dans le panneau Properties.
  - Demonstration: creer des keyframes sur un objet, lancer la lecture et montrer l'interpolation.
7. **Animation du graphe de scene (Transformation)**
  - Generation de demos d'animation sur selection avec animation du parent et de l'enfant.
  - Les animations sont evaluees pendant la mise a jour, incluant les hierarchies.
  - Demonstration: lancer "Generer demo sur selection", puis "Play toutes anims".
8. **Point de vue camera (Camera)**
  - Camera orbitale/zoom autour d'une cible et camera de jeu (modes suivi/premiere personne).
  - Demonstration: orbite en mode edition + suivi en mode Play.
9. **Modes de projection (Camera)**
  - Support des projections perspective et orthographique.
  - Demonstration: basculer de mode et comparer la perception de profondeur.
10. **Modeles d'illumination (Illumination classique)**
  - Support de trois modeles: Lambert, Phong, Blinn-Phong.
  - Selection du modele par objet/materiau.
  - Demonstration: meme objet rendu selon les 3 modeles.
11. **Lumieres multiples dynamiques (Illumination classique)**
  - Jusqu'a 4 lumieres configurees (position, couleur, intensite, attenuation, activation).
  - Parametrage en direct dans le panneau "Lumieres".
  - Demonstration: activer/desactiver des lumieres et ajuster intensite/attenuation.
12. **Coordonnees de texture (Texture)**
  - Utilisation des UV des modeles importes pour le texturage.
  - Chargement/retrait de texture par objet depuis l'interface.
13. **Texture procedurale (Texture)**
  - Option de texture procedurale (damier) avec echelle ajustable.
  - Demonstration: alterner texture fichier vs texture procedurale.
14. **Sauvegarde/chargement de scene (fonctionnalite complementaire)**
  - La scene est serialisable (JSON), incluant etats d'animation et keyframes.
  - Permet de recharger une scene et retrouver les objets/parametres.
  - Demonstration: sauvegarder, vider/modifier, puis recharger.
15. **Histogramme d'image (Image)**
  - Fonction "Calculer histogramme" disponible dans l'interface.
  - Demonstration: calcul sur image active et affichage du resultat dans l'UI.

### Criteres partiels ou a confirmer pour la remise

- **Types de lumiere differents**: le projet gere clairement plusieurs instances de lumiere; valider si cela couvre 4 *types* distincts selon la definition stricte de l'enonce.
- **Import/export d'images**: un calcul d'histogramme est present; verifier si un pipeline complet d'import/export image est revendique.
- **Effets avances** (cubemap, portail, occlusion): non identifies clairement dans le code principal actuel.
- **Boite de delimitation**: affichage de BBox present; documenter explicitement les cas couverts en demo.

### Format de presentation conseille pour chaque critere

- **Description courte**: comment le critere est realise.
- **Demonstration**: quoi montrer dans la video.
- **Extrait technique**: classe/fonction cle.
- **Limites connues**: ce qui reste partiel.

### Extraits techniques a citer dans le rapport (suggestion)

- `Sandbox/src/SandboxApp.cpp`: `Undo()`, `Redo()`, piles `m_UndoStack` / `m_RedoStack`.
- `Sandbox/src/SandboxApp.cpp`: `KeyframeTRS`, `SampleKeys(...)`, `UpdateClassicAnimations(...)`.
- `Sandbox/src/SandboxApp.cpp`: UI keyframes (`Ajouter keyframe`, edition de `t/Pos/Rot/Scale`, `Play/Pause/Stop`).
- `Sandbox/src/SandboxApp.cpp`: import (`Importer OBJ`, `Importer FBX`, `LoadMeshFile(...)`).
- `Sandbox/src/SandboxApp.cpp`: eclairage multiple (`m_Lights[4]`, panneau `Lumieres`).

---

## 3.7 Ressources (1 page)

### Ressources produites par l'equipe (a remplir)

- Modeles 3D originaux: `TODO`
- Textures originales: `TODO`
- Shaders originaux: `TODO`
- Scripts/outils internes: `TODO`
- Captures/video de demonstration: `TODO`

### Ressources externes et references (a remplir)

- Bibliotheques: GLFW, Glad, ImGui, GLM, Assimp, stb_image, spdlog.
- Assets externes (modeles/textures/audio): `TODO ajouter URL/licence/source`.

### IA et divulgation (obligatoire selon enonce)

L'equipe a utilise l'IA pour assistance a la redaction/documentation et/ou generation de code: `OUI/NON`.  
Description de l'utilisation: `TODO preciser de facon transparente`.

---

## 3.8 Presentation de l'equipe (1 page)

### Membres

- `Nom 1` - `matricule` - `role principal`
- `Nom 2` - `matricule` - `role principal`
- `Nom 3` - `matricule` - `role principal`

### Repartition des taches (exemple)

- **Moteur et rendu**: `TODO`
- **Interface et outils d'edition**: `TODO`
- **Import assets et pipeline de donnees**: `TODO`
- **Tests, stabilisation, video et livraison**: `TODO`

### Bilan d'equipe (a remplir)

- Principaux defis techniques: `TODO`
- Solutions retenues: `TODO`
- Ce qui serait ameliore avec plus de temps: `TODO`

---

## Annexes (optionnel mais recommande)

- Captures d'ecran des fonctionnalites.
- Diagrammes UML/schemas architecture.
- Tableau de correspondance `criteres -> evidence video`.
- Liste de verification avant remise:
  - compilation propre sur un autre PC;
  - aucun crash au demarrage;
  - video lisible ou lien fonctionnel;
  - PDF final present dans `/doc`;
  - archive `.zip` avec nomenclature demandee.

