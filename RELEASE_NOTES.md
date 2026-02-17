# Release 1.2.0

- Fixed Home status/profile line truncation: now uses full available width and elides only on real overflow.
- Added centralized typography layer with platform-aware font family fallback and DPI-aware pixel sizing.
- Normalized key font weights (Regular/Medium/Semibold) to prevent overly bold Linux rendering.
- Reworked `ConnectButton` sizing: circle diameter and text size now adapt to font metrics and localized state labels.
- Updated release pipeline for Linux Docker build to always receive version from tag via `APP_VERSION_STR`.
