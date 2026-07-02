# Merge locale con origin/distribution

Data: 2026-07-01

Branch: `distribution`

Base locale prima del merge: `9ddf8b0`

Remote integrato: `origin/distribution` fino a `ff17ff3`

Patch di sicurezza delle modifiche locali prima del merge:

`E:\VisionSystemCPP\.codex_merge\local-uncommitted-before-merge.patch`

## Commit remoti integrati

- `c265fe9` Add camera backend profile bridge
- `c31f63d` Switch Vimba trigger mode for production
- `601885d` Use Line3 hardware trigger for Alvium
- `f0f3b4e` Avoid repeated USB resolution probing
- `2963a67` Avoid live surface diagnostic preview jump
- `daf226a` Keep live setup preview at native frame size
- `65e892b` Preserve runtime camera assignments during build
- `ef967f6` Harden camera persistence and clean recipe creation
- `ff17ff3` Add auto geometry assistant experiment

## Conflitti risolti

- `CMakeLists.txt`: mantenuti i comandi remoti che seminano `config` e copiano `translations`, `docs/help` e `ollama/vision-help` nel runtime.
- `src/gui/modules/setup/MainWindowSetupModule_Runtime.cpp`: mantenuta la preview tile ridimensionata a `1920x1080`; la preview grande resta generata separatamente a risoluzione piena.

## Stato finale atteso

Il branch locale e' allineato a `origin/distribution`.

Le modifiche locali restano non committate nel working tree.
