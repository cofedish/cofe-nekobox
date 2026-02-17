# Typography Diagnostic and Fix Plan

## 1) UI stack and font/style entry points

- UI stack: **Qt Widgets** (`.ui` forms + custom widgets), no QML.
- Base app font (before fix): `main/main.cpp` (manual family fallback + `setPointSize(10)`).
- Theme/QSS styles: `ui/ThemeManager.cpp` (`BuildThemeQss()` with many font-size/font-weight values).
- Connect center button drawing: `ui/widget/ConnectButton.cpp` (`paintEvent`, custom circle + text rendering).
- Home status/profile text on Home: `ui/mainwindow.cpp` (`refresh_status()` updates `label_running`).

## 2) Reproduced/root causes from code diagnostics

- Home truncation was caused by two issues:
  - hard cut in code: `QString(...).left(30)` in `refresh_status()`;
  - tight layout behavior from `home_center` constraints and non-elided plain `QLabel` text.
- Linux typography inconsistency causes:
  - heavy weight defaults in QSS (`font-weight: 600` on key labels);
  - custom `ConnectButton` text explicitly forced to bold;
  - button geometry fixed around 200x200 without text-metric-driven sizing.

## 3) Runtime font inspection (QFontInfo)

- Added startup debug output in `main/main.cpp`:
  - `qDebug() << "[typography] app" << Typography::FontDebugString(a.font());`
- `Typography::FontDebugString()` reports requested vs actual font (`family`, `weight`, `pixelSize`) via `QFontInfo`.
- On Linux (deb/AppImage), use app logs and verify actual family chosen from fallback chain:
  - preferred: `Noto Sans`, then `DejaVu Sans`, then fallback.

## 4) What was fixed

- Introduced centralized typography layer: `ui/Typography.hpp/.cpp`.
- Switched to DPI-aware `pixelSize` based sizing and explicit Regular/Medium/Semibold usage.
- Removed early text clipping path on Home and implemented width-based elide.
- Reworked `ConnectButton` sizing and font adaptation from `QFontMetrics`.
