# CofeBox Migration Notes

## Scope
This release migrates product branding and runtime paths from legacy `nekoray/nekobox` names to `cofebox`.

## Config/Data Directory Migration
At startup (AppData mode), CofeBox now uses:
- Linux: `~/.config/cofebox`
- Windows: `%APPDATA%/cofebox`

If the new directory is empty, CofeBox automatically checks legacy locations:
- `~/.config/nekoray`, `~/.config/NekoRay`
- `~/.config/nekobox`, `~/.config/NekoBox`
- Windows equivalents under `%APPDATA%`

When a legacy directory is found:
1. A backup copy is created next to the legacy directory:
   - `<legacy-dir>.cofebox-backup-<timestamp>`
2. Data is copied into the new `cofebox` directory.

## Data File Migration
The primary group store was renamed:
- old: `groups/nekobox.json`
- new: `groups/cofebox.json`

On first run, if `groups/cofebox.json` is missing and `groups/nekobox.json` exists, CofeBox copies old data into the new filename.

## Core/Binary Renames
- GUI binary: `cofebox(.exe)`
- Core binary: `cofebox_core(.exe)`
- Updater: `cofebox-updater(.exe)`

Linux package/runtime metadata also switched to `cofebox`:
- package id: `cofebox`
- desktop file: `cofebox.desktop`
- install root: `/opt/cofebox`

