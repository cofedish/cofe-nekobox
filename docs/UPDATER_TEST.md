# Auto Updater Test Plan

## Preconditions
- Build/release contains:
  - `cofebox-updater` (`.exe` on Windows)
  - `sha256sums.txt` in GitHub Release assets.
- Test target has internet access to:
  - `https://api.github.com/repos/<owner>/<repo>/releases/latest`
  - `https://github.com/<owner>/<repo>/releases/*`

## 1) Windows portable ZIP
1. Install older portable build into writable folder.
2. Start app, open `About`, click `Check updates`.
3. Verify state flow in UI: `Checking -> Update available`.
4. Click `Update to vX.Y.Z`.
5. Verify state flow: `Downloading -> Verifying -> Installing`.
6. App exits, updater replaces files, app restarts automatically.
7. Confirm new version in `About`.
8. Confirm `logs/updater.log` contains:
  - release detection
  - checksum verification OK
  - helper start.

## 2) Linux AppImage (writable path)
1. Put AppImage into writable directory (e.g. `~/Downloads`), run it.
2. Open `About`, click `Check updates`, then `Update`.
3. Confirm no modal blocking during check/download.
4. After install, app restarts from updated AppImage.
5. Verify backup exists as `<AppImage>.bak` and new AppImage is executable.

## 3) Linux AppImage (non-writable path)
1. Put AppImage into non-writable location (e.g. `/opt` without write rights), run it.
2. Click `Check updates`.
3. Verify UI reports auto-update unavailable for current location.
4. Verify `Open release page` button is visible and works.
5. Verify no install attempt is started.

## 4) Linux DEB
1. Install older `.deb` package.
2. Open `About`, click `Check updates`, then `Update`.
3. Verify `pkexec` prompt appears.
4. After successful install, app shows restart prompt.
5. Click restart and confirm new version.

## 5) Security checks
1. Remove `sha256sums.txt` from test release (or point to release without it).
2. Verify app does **not** auto-install and shows fallback to release page.
3. Corrupt downloaded asset hash (or checksum value).
4. Verify update fails in `Verifying` state and install is not executed.
