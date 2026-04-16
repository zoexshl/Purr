# Script video IMN401 (sans micro)

Objectif: demontrer clairement les criteres fonctionnels implementes, sans voix, avec texte a l'ecran.

Duree cible: **4 min 30 s a 6 min**
Format recommande: **1080p, 30 FPS, MP4 (H.264)**

---

## 1) Timeline prete a suivre

## 0:00 - 0:12 | Ecran titre

Texte ecran:

- `Purr - Projet de session IMN401`
- `Etudiant: [Ton nom] ([matricule])`
- `Session Hiver 2026`

Visuel:

- Logo/scene fixe propre dans le viewport.

---

## 0:12 - 0:25 | Sommaire des criteres montres

Texte ecran:

- `Criteres demontres:`
- `Import 3D, Graphe de scene, Selection multiple, Gizmos`
- `Undo/Redo, Animation keyframe, Camera, Projection`
- `Illumination, Lumieres, Textures, Sauvegarde/Chargement`

Visuel:

- UI ouverte avec scene de base.

---

## 0:25 - 0:55 | Critere 1 - Modeles 3D importes

Action:

1. Cliquer `Importer OBJ`, charger un modele.
2. Cliquer `Importer FBX` (ou dossier FBX), charger un autre modele.

Texte ecran:

- `Critere: Modeles 3D importes (OBJ/FBX)`
- `Import interactif via interface`

Preuve visible:

- Nouveaux objets apparaissent dans scene/hierarchie.

---

## 0:55 - 1:20 | Critere 2 - Graphe de scene + selection multiple

Action:

1. Dans `Scene`, montrer parent/enfant (ou grouper sous pivot).
2. Selectionner plusieurs objets avec `Ctrl`.
3. Montrer l'etat de selection dans la liste.

Texte ecran:

- `Criteres: Graphe de scene + Selection multiple`

Preuve visible:

- Lien hierarchique et multi-selection claire.

---

## 1:20 - 1:50 | Critere 3 - Transformations interactives (gizmos)

Action:

1. `T`: deplacer un objet (axes).
2. `R`: rotation.
3. `S`: echelle.

Texte ecran:

- `Critere: Transformations interactives`
- `Gizmos Translate / Rotate / Scale`

Preuve visible:

- Changement direct dans viewport.

---

## 1:50 - 2:15 | Critere 4 - Undo/Redo

Action:

1. Faire 2-3 transformations evidentes.
2. `Ctrl+Z` plusieurs fois.
3. `Ctrl+Y` pour refaire.
4. Optionnel: cliquer boutons `< Undo` et `> Redo`.

Texte ecran:

- `Critere: Historique de transformation (Undo/Redo)`

Preuve visible:

- L'objet revient puis reprend les etats precedents.

---

## 2:15 - 2:55 | Critere 5 - Animation keyframe (classique)

Action:

1. Ouvrir `Properties` d'un objet.
2. Activer animation.
3. Ajouter 3 keyframes (temps + pos/rot/scale differents).
4. `Play`, puis `Pause`, puis `Stop`.
5. Montrer le `Slider Temps`.

Texte ecran:

- `Critere: Animation classique (keyframes)`
- `Interpolation TRS + controle playback`

Preuve visible:

- Mouvement fluide entre keyframes + controls.

---

## 2:55 - 3:20 | Critere 6 - Camera + mode projection

Action:

1. Orbit/zoom dans la scene.
2. Basculer `Perspective` <-> `Orthographique`.

Texte ecran:

- `Criteres: Point de vue camera + Modes de projection`

Preuve visible:

- Difference nette de perception de profondeur.

---

## 3:20 - 3:55 | Critere 7 - Illumination + lumieres multiples

Action:

1. Dans `Lumieres`, activer/desactiver plusieurs lumieres.
2. Changer intensite/couleur/attenuation.
3. Changer modele d'illumination d'un objet (Lambert/Phong/Blinn-Phong).

Texte ecran:

- `Criteres: Illumination classique + Lumieres multiples`

Preuve visible:

- Impact visuel immediat sur les objets.

---

## 3:55 - 4:20 | Critere 8 - Textures (fichier + procedurale)

Action:

1. Charger une texture sur un objet.
2. Activer texture procedurale damier + modifier echelle.

Texte ecran:

- `Criteres: Coordonnees de texture + Texture procedurale`

Preuve visible:

- Difference claire entre texture image et damier procedural.

---

## 4:20 - 4:45 | Critere 9 - Sauvegarde / chargement scene

Action:

1. `Sauver scene`.
2. Modifier la scene (deplacement/suppression rapide).
3. `Charger scene` pour restaurer.

Texte ecran:

- `Fonctionnalite complementaire: Sauvegarde/Chargement JSON`

Preuve visible:

- Retour a l'etat sauvegarde.

---

## 4:45 - 5:00 | Outro

Texte ecran:

- `Projet realise individuellement`
- `Merci du visionnement`

Visuel:

- Plan final propre de la scene.

---

## 2) Checklist anti-oubli (avant export video)

- Import OBJ montre
- Import FBX montre
- Hierarchie/parent-enfant visible
- Selection multiple (`Ctrl`) visible
- Translate/Rotate/Scale montres
- Undo/Redo clavier (ou boutons) montre
- Creation de keyframes montre
- Play/Pause/Stop animation montre
- Camera orbit/zoom montre
- Perspective + Orthographique montres
- Lambert/Phong/Blinn-Phong montres
- Lumieres multiples (activation + parametres) montrees
- Texture fichier + texture procedurale montrees
- Sauvegarde + chargement scene montres
- Texte `projet individuel` present en fin de video

---

## 3) Conseils montage rapides

- Garde les textes courts (1 a 2 lignes max).
- Fais des transitions simples (fondu court).
- Mets musique faible, sans couvrir les sons utiles si tu les gardes.
- Si une action rate en direct, recoupe: priorite a la clarte.
- Export final: MP4 H.264, verifier lecture sur un autre PC.

