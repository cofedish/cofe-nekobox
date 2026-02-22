# Release 1.4.3

- Linux zip: fixed right-click behavior on tray icon (context menu now opens reliably).
- Linux zip: fixed drawer/sidebar scrollbar styling in System theme (no broken black track).
- Fixed app icon fallback chain for window title/taskbar/tray to prevent incorrect placeholder icon.

# Release 1.2.0

- Fixed Home status/profile line truncation: now uses full available width and elides only on real overflow.
- Added centralized typography layer with platform-aware font family fallback and DPI-aware pixel sizing.
- Normalized key font weights (Regular/Medium/Semibold) to prevent overly bold Linux rendering.
- Reworked `ConnectButton` sizing: circle diameter and text size now adapt to font metrics and localized state labels.
- Updated release pipeline for Linux Docker build to always receive version from tag via `APP_VERSION_STR`.

## Security Updates

- Fixed secret leakage risk: external process startup no longer logs full environment values.
- Fixed TLS handling for subscriptions: insecure TLS is now per-subscription (not global), with explicit warning in group settings.
- Hardened Linux TUN privilege boundary: added VPN script integrity verification and restricted privileged shell startup (`--noprofile --norc`, cleaned env).
- Restricted subscription update URLs to HTTP/HTTPS only; unsafe schemes are rejected.
- Updated default test URLs from HTTP to HTTPS.
