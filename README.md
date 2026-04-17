# Purr - Projet IMN401

Application d'infographie 3D en C++/OpenGL, avec un mode editeur interactif (`Sandbox`) construit sur un moteur maison (`Purr`).

Ce depot contient:
- le moteur (`Purr/src`)
- l'application cliente (`Sandbox/src`)
- les dependances tierces (`Purr/vendor`)
- la documentation du projet (`doc`)

## Fonctionnalites principales

- Edition de scene 3D avec hierarchie d'objets (parent/enfant)
- Selection simple et multiple
- Gizmos interactifs: translation, rotation, echelle
- Undo/Redo (`Ctrl+Z` / `Ctrl+Y`)
- Animation keyframe (TRS), lecture/pause/stop, loop
- Import de modeles (OBJ, FBX via Assimp)
- Textures (fichier + procedurale damier)
- Eclairage dynamique multi-sources
- Camera orbitale et projection perspective/orthographique
- Sauvegarde et chargement de scene (JSON)

## Stack technique

- C++17
- OpenGL + GLSL
- GLFW
- Glad
- ImGui
- GLM
- Assimp
- stb_image
- spdlog
- Premake5

## Prerequis

- Windows 10/11 x64
- Visual Studio 2022 (Desktop development with C++)
- Premake5 disponible dans le `PATH` (ou script equivalent de l'equipe)

## Compilation (Windows)

1. Ouvrir un terminal a la racine du projet.
2. Generer les fichiers Visual Studio:
   - `premake5 vs2022`
3. Ouvrir la solution generee.
4. Choisir `x64` + configuration `Debug` (ou `Release`).
5. Compiler la solution.
6. Lancer le projet de demarrage `Sandbox`.

Notes:
- Les DLL Assimp sont copiees automatiquement en post-build selon la configuration.
- En cas de probleme de chemins assets, executer `Sandbox` depuis le dossier de sortie standard.

## Structure du projet

```text
Purr/
  Purr/                 # Moteur (static lib)
    src/Purr/           # Core, Renderer, Events, ImGui, Loaders
    vendor/             # Dependencies tierces
  Sandbox/              # Application cliente (exe)
    src/                # Scene editor + logique de demo
  doc/                  # Documentation du projet (rapport, etc.)
  premake5.lua          # Build configuration
```

## Controles utiles (editeur)

- `T` : mode Translate
- `R` : mode Rotate
- `S` : mode Scale
- `Ctrl + Z` : Undo
- `Ctrl + Y` : Redo
- Clic gauche dans viewport : selection / manipulation selon contexte

## Documentation de remise

- Rapport principal: `purr_description_projet.pdf` 
- Video youtube :   `https://www.youtube.com/watch?v=2ytG-AieKYk `


