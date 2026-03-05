# Release 1.6.0

- Sidebar now renders every real top-level page from `stacked_pages` instead of the reduced 5-item drawer.
- Added second-level navigation for server groups, log tabs, and page-specific actions without removing any existing product sections.
- Replaced menu and quick-action icons with theme-aware vector glyphs driven by palette tokens for normal, hover, and active states.
- Reworked the animated background shader to render sharper silk ribbon bands with edge highlights, thickness, and layered depth instead of blurred fog.
- Unified card radius/borders and refined the central connect orb glow/pulse for the new release polish.

# Release 1.5.1

- Full redesign of the main window to a premium glass/neon dashboard style.
- New Home layout: central connect orb, server/profile cards, right-side modes and statistics panel.
- Upgraded animated background (space gradient, ribbon waves, star field) with `Reduce motion` support.
- Refined `ConnectButton` visuals and animation states for a cleaner connected/connecting experience.
- Unified redesign behavior across all built-in themes: `System`, `Light`, `Dark`, `Lucifer`, `BAD600 Light`, `BAD600 Dark`.
- Updated theme typography fallback chain for a more consistent premium look on Windows/Linux.

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
