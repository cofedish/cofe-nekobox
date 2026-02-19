# Release 1.2.0

- Fixed Home status/profile line truncation: now uses full available width and elides only on real overflow.
- Added centralized typography layer with platform-aware font family fallback and DPI-aware pixel sizing.
- Normalized key font weights (Regular/Medium/Semibold) to prevent overly bold Linux rendering.
- Reworked `ConnectButton` sizing: circle diameter and text size now adapt to font metrics and localized state labels.
- Updated release pipeline for Linux Docker build to always receive version from tag via `APP_VERSION_STR`.

# Release 1.2.2

- Added full auto-update service with async startup checks and About-page controls.
- Added release integrity enforcement via `sha256sums.txt`; installation is blocked without checksum verification.
- Added platform-aware automatic installation flows:
  - Windows portable ZIP via helper with backup and rollback.
  - Linux AppImage replacement via helper with backup and rollback.
  - Linux `.deb` install via `pkexec apt install`.
- Added updater logs file and About-page button to open update logs.
- Added release pipeline step to publish `sha256sums.txt` with all release assets.
