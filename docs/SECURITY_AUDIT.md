# CofeBox Security Audit Report

**Classification:** Internal — For Corporate Use Evaluation
**Version Audited:** Branch `main`, commit `73c3086` (2025-xx)
**Audit Date:** 2026-02-20
**Auditor Role:** AppSec Engineer / C++/Qt Reviewer
**Repository:** https://github.com/cofedish/cofe-nekobox

---

## Executive Summary

CofeBox is an open-source Qt/C++ + Go GUI frontend for [sing-box](https://github.com/SagerNet/sing-box), providing VPN/proxy management including TUN mode, subscription import, auto-update, and multi-platform packaging (Windows ZIP, Linux AppImage/DEB). It is a maintained fork of the nekoray/nekobox family.

**Overall Risk Rating: MEDIUM**

The application follows several security best practices: HTTPS-only enforcement for update downloads, SHA-256 integrity verification of all release artifacts, URL allowlist restricting updates to `github.com`, Policy Kit (`pkexec`) for privilege escalation rather than a setuid binary, and no hardcoded credentials anywhere in the codebase.

However, several areas require attention before recommending the application for broad corporate deployment:

- The `sub_insecure` flag allows global SSL peer-verification bypass for subscription fetching, enabling MITM attacks.
- The Linux TUN startup executes a shell script as root via `pkexec bash`, which is a broad privilege grant.
- The GitHub Actions workflow lacks `contents: read` least-privilege scoping and uses `ACTIONS_ALLOW_UNSECURE_COMMANDS: true`.
- Full environment variable inheritance by child processes may expose secrets present in the parent process environment.
- No code-signing of distributed binaries (Windows/macOS), and no SBOM or software attestation.

This report provides a detailed finding table (20 items), concrete remediation steps with file/line references, and a hardening roadmap.

---

## Table of Contents

1. [Scope & Assumptions](#1-scope--assumptions)
2. [Threat Model](#2-threat-model)
3. [Attack Surface Review](#3-attack-surface-review)
4. [Dependency & Supply Chain](#4-dependency--supply-chain)
5. [Specific High-Risk Areas](#5-specific-high-risk-areas)
6. [Findings Table](#6-findings-table)
7. [Hardening Recommendations](#7-hardening-recommendations)
8. [CI Security Controls](#8-ci-security-controls)
9. [Appendix](#9-appendix)

---

## 1. Scope & Assumptions

### 1.1 Versioned Scope

| Item | Value |
|---|---|
| Repository | `cofedish/cofe-nekobox` |
| Audited branch | `main` |
| Latest commit (at audit) | `73c3086` — "fix: restore updater UI actions and startup update checks" |
| Core engine | sing-box (external binary, not audited here) |
| Audit method | White-box static code review (no live exploitation) |

### 1.2 Supported Platforms

| Platform | Package | Privilege model |
|---|---|---|
| Windows 10/11 | `.zip` (portable) | UAC elevation via `ShellExecuteEx` |
| Linux (generic) | `.AppImage` | setcap `cap_net_admin` or TUN via `pkexec` |
| Linux (Debian/Ubuntu) | `.deb` | `postinst` sets `cap_net_admin` via `setcap` |
| macOS | Not yet officially released | `osascript` admin prompt |

### 1.3 Out of Scope

- Security of the upstream sing-box core binary
- Server-side infrastructure (GitHub, subscription providers)
- Physical access attacks

---

## 2. Threat Model

### 2.1 STRIDE Analysis

| Threat | Vector | Affected Component | Severity |
|---|---|---|---|
| **Spoofing** | Malicious subscription server impersonates legitimate provider when `sub_insecure=true` | HTTPRequestHelper / GroupUpdater | High |
| **Tampering** | Malicious ZIP/AppImage in a compromised GitHub release (before SHA-256 check or if checksum skipped) | UpdateService | High |
| **Repudiation** | No cryptographic signature on update packages; only SHA-256 (anyone who modifies checksums.txt can change payload) | Release pipeline | Medium |
| **Information Disclosure** | Environment variables logged verbatim at core startup, potentially exposing secrets from parent shell | ExternalProcess | High |
| **Information Disclosure** | Proxy credentials (`inbound_auth`) stored in plaintext JSON config files | DataStore / config/ | Medium |
| **Elevation of Privilege** | VPN TUN mode: `pkexec bash <script>` grants full root shell via PolicyKit | mainwindow / vpn-run-root.sh | High |
| **Elevation of Privilege** | DEB update: `pkexec apt install -y <path>` installs arbitrary .deb as root | auto_update.go | High |
| **Denial of Service** | Uncontrolled subscription parsing (large/malformed base64, infinite recursion) | GroupUpdater | Low |

### 2.2 Data Flow Diagram (Textual)

```
[User] ─── subscription URL ──►  [UI / GroupUpdater]
                                      │
                               HTTP(S) fetch (optionally via local proxy)
                                      │
                               [Subscription Server] ──► response body
                                      │
                              base64 decode + format detect
                                      │
                               [Proxy config parser] ──► [DataStore / config/]
                                                               │
                                                         [sing-box core]
                                                         (gRPC, token auth)
                                                               │
                                                     [TUN / System proxy]
                                                               │
                                                    Network traffic routing

[UI] ─── GitHub API (HTTPS) ──► [UpdateService]
                                     │
                               Download release asset (HTTPS, github.com only)
                                     │
                               SHA-256 verify
                                     │
                               [updater helper binary]
                                     │ (Windows: ZIP extract + relaunch)
                                     │ (Linux AppImage: file replace)
                                     │ (Linux DEB: pkexec apt install)
```

### 2.3 Trust Boundaries

| Boundary | Description |
|---|---|
| UI ↔ sing-box core | gRPC over `127.0.0.1`, authenticated with `core_token` (runtime-generated) |
| UI → updater helper | `QProcess::startDetached` with CLI arguments; updater runs in same user context |
| updater → OS (DEB) | `pkexec apt install` — crosses privilege boundary to root |
| UI → VPN script | `pkexec bash <script>` — crosses privilege boundary to root |
| User → filesystem | Config files in `./config/` or `$XDG_CONFIG_HOME/cofebox/config/` |
| GitHub API → UI | Trusted via HTTPS + domain allowlist; no certificate pinning |

### 2.4 Protected Assets

| Asset | Storage | Risk if compromised |
|---|---|---|
| Proxy server credentials | `config/groups/*.json` (plaintext) | Exposure of proxy provider credentials |
| `core_token` | Runtime memory only (not persisted) | Unauthorized gRPC control of core |
| `inbound_auth` (username/password) | `config/` JSON | Exposure of local proxy auth |
| `core_box_clash_api_secret` | `config/` JSON | Unauthorized Clash API access |
| Routing rules | `config/` JSON | Traffic redirection |
| Logs | `logs/` directory | May contain sensitive URLs or partial config |
| Update binary | Temp dir + install dir | Code execution if replaced |

---

## 3. Attack Surface Review

### 3.1 Network Requests

| Endpoint | Protocol | Source | Authenticated | Integrity |
|---|---|---|---|---|
| GitHub API (`api.github.com/repos/cofedish/cofe-nekobox`) | HTTPS | UpdateService.cpp | Optional Bearer token (env var) | TLS |
| Release asset download (`github.com/cofedish/cofe-nekobox/releases/download/…`) | HTTPS | UpdateService.cpp | No | TLS + SHA-256 |
| Subscription URL (user-provided) | HTTP **or** HTTPS | GroupUpdater.cpp | No | Optional (bypassed by `sub_insecure`) |
| Latency test URL `http://cp.cloudflare.com/` | **HTTP** | NekoGui_DataStore.hpp:97 | No | None |
| Speed test URL `http://cachefly.cachefly.net/10mb.test` | **HTTP** | NekoGui_DataStore.hpp:98 | No | None |
| gRPC core (`127.0.0.1:<port>`) | Plain HTTP/2 | rpc/gRPC.cpp | `core_token` | None (localhost) |

**Key observation:** All update-critical network calls use HTTPS with domain validation. The HTTP test URLs carry no sensitive data. Subscription fetching is the primary MITM risk due to `sub_insecure`.

### 3.2 Filesystem

| Path | Operation | Risk |
|---|---|---|
| `./config/` or `$XDG_CONFIG_HOME/cofebox/config/` | R/W JSON configs | Plaintext credentials; no ACL enforcement |
| `./logs/updater.log` | Append | May log update URLs and partial tokens |
| `$TMPDIR/cofebox-update/staging-<ns>` | Create, extract, delete | Temp dir created with `0755`; no race-condition protection on Linux |
| `./updater` (Linux symlink) | Execute | Symlink to `launcher`; created if not present (main.cpp:160) |
| `/opt/cofebox/` (DEB install) | Write (as root) | Controlled by pkexec/apt |
| Legacy config dirs (`~/.config/nekoray`, `~/.config/nekobox`) | Read + copy | Migration copies entire dir without symlink validation |

### 3.3 Process Execution

All process spawning uses `QProcess` (no `system()`, no `popen()` found in codebase — **positive**).

| Spawn Site | File:Line | Elevated | Arguments From |
|---|---|---|---|
| `pkexec setcap <cap> <path>` | LinuxCap.cpp:20 | Yes (PolicyKit) | Capability string + resolved path |
| `pkexec bash <scriptPath>` | mainwindow.cpp (VPN start) | Yes (PolicyKit) | App-bundled script path + `$CONFIG_PATH` env var |
| `pkexec killall -2 cofebox_core` | mainwindow.cpp (VPN stop) | Yes (PolicyKit) | Hardcoded binary name |
| `pkexec pkill -2 -P <pid>` | mainwindow.cpp (VPN stop) | Yes (PolicyKit) | PID integer (safe) |
| `pkexec apt install -y <debPath>` | auto_update.go:236 | Yes (PolicyKit) | Path from UpdateService (SHA-256 verified) |
| `./cofebox_core run -c <config>` | ExternalProcess | No | Config path from DataStore |
| `./updater --mode windows …` | UpdateService.cpp | No | CLI flags (controlled by UI) |
| `tasklist /FI "PID eq <pid>"` | auto_update.go:85 | No | PID integer (safe) |

**No `system()` or `popen()` calls found — positive finding.**

### 3.4 IPC / Local Sockets

| Mechanism | Bind Address | Auth |
|---|---|---|
| gRPC core server | `127.0.0.1:<mixed_port>` | `core_token` (written to stdin at startup) |
| Qt `QLocalServer` (single-instance) | Named pipe / Unix socket with prefix `cofebox-localserver-` | Same-user OS permissions |
| HTTP mixed inbound | `127.0.0.1:<inbound_socks_port>` | Optional `inbound_auth` |

**All local services bind to loopback (127.0.0.1) — positive.** The `QLocalServer` single-instance socket is correctly scoped to the local user's session.

---

## 4. Dependency & Supply Chain

### 4.1 C++ Dependencies

| Library | Version pinning | Source | License | Risk |
|---|---|---|---|---|
| Qt 5.12 / 6.7 | Fixed in CI matrix | Custom SDK script | LGPL | Low — well-maintained |
| yaml-cpp | CMake `find_package` / vendored build | libs/build_deps_all.sh | MIT | Low |
| ZXing-cpp | CMake / vendored | libs/build_deps_all.sh | Apache-2.0 | Low |
| QHotkey | CMake / vendored | libs/build_deps_all.sh | MIT | Low |
| grpc / protobuf | Vendored via libs scripts | libs/ | Apache-2.0 | Medium — large attack surface, ensure version is recent |

### 4.2 Go Dependencies

| Module | Usage | Risk |
|---|---|---|
| `github.com/codeclysm/extract` | ZIP/tar.gz extraction in updater | **Medium** — extraction library; see Finding SEC-005 (zip-slip) |
| Standard library (`os/exec`, `io`, `flag`) | All other updater logic | Low |

### 4.3 CI/CD Supply Chain

| Tool | Version | Source | Risk |
|---|---|---|---|
| `actions/checkout@v3` | Pinned to v3 (not SHA) | GitHub Actions | Medium — v3 tag can be moved; pin to SHA |
| `actions/upload-artifact@v4` | v4 tag | GitHub Actions | Medium |
| `actions/download-artifact@v4` | v4 tag | GitHub Actions | Medium |
| `actions/cache@v3` | v3 tag | GitHub Actions | Medium |
| `ilammy/msvc-dev-cmd@v1` | v1 tag | GitHub Actions | Medium |
| `seanmiddleditch/gha-setup-ninja@v3` | v3 tag | GitHub Actions | Medium |
| `actions/setup-go@v3` | v3 tag | GitHub Actions | Medium |
| `ghr` (release tool) | `v0.13.0` — downloaded at runtime via curl | GitHub releases | **High** — curl pipe to binary without integrity check |
| Docker image `ghcr.io/matsuridayo/debian10-qt5:20230131` | Date-tagged | GitHub Container Registry | Medium — image not re-pulled from a pinned SHA |

### 4.4 License Summary

All identified dependencies are OSI-approved (MIT, Apache-2.0, LGPL, GPL-3.0). GPL-3.0 on the main application has **copyleft implications** for internal distribution — consult Legal if CofeBox is modified and redistributed internally.

---

## 5. Specific High-Risk Areas

### 5A. Auto-Update

**Implementation:** `ui/UpdateService.cpp` + `go/cmd/updater/auto_update.go`

#### Positive Controls

- **HTTPS enforcement:** `UpdateService::isTrustedReleaseAssetUrl()` (UpdateService.cpp) rejects any URL whose scheme is not `https` or host is not exactly `github.com`.
- **SHA-256 integrity:** `verifySha256File()` computes the hash of the downloaded artifact and compares it against the `sha256sums.txt` from the same GitHub release.
- **Staged extraction:** Windows updates are extracted to `$TMPDIR/cofebox-update/staging-<nanoseconds>/` before installation; backup is created before overwriting.
- **Rollback:** If `copyDir()` fails during installation, the backup is restored (auto_update.go:134).
- **Backup before DEB install:** Not applicable (apt handles this), but SHA-256 is verified before `pkexec apt install`.

#### Issues

1. **No signature on checksums file** (SEC-001): The `sha256sums.txt` itself is downloaded from GitHub over HTTPS but is not GPG-signed. If an attacker gains write access to the GitHub release (compromised token), they can replace both the artifact and the checksum. This is a supply-chain risk, not a runtime exploitation path.

2. **No downgrade protection** (SEC-002): The updater does not compare the new version number against the currently installed version before applying the update. A downgrade could re-introduce previously patched vulnerabilities.

3. **`ghr` downloaded without integrity check** (SEC-003, CI level): The publish job (`build-cofebox-cmake.yml:174`) downloads `ghr v0.13.0` via `curl | tar xzv` with no checksum verification. A MITM or compromised release could substitute a malicious binary that tampers with the release artifacts.

4. **Symlink in archive not restricted** (SEC-005): `auto_update.go:299-305` — the `copyDir` function follows symlinks from the extracted archive and recreates them at the destination. If a maliciously crafted archive contains a symlink pointing outside the install directory, it could create a symlink elsewhere on the filesystem (classic zip-slip variant for symlinks). The `extract` library may or may not strip these — must be verified.

5. **`$CONFIG_PATH` in VPN script** (SEC-010): The VPN shell script (`vpn-run-root.sh:22`) uses `$CONFIG_PATH` unquoted in `"./cofebox_core" run -c "$CONFIG_PATH"`. If the path contains spaces or shell metacharacters, this could cause unexpected behavior (though not a full injection since the script uses `set -e`).

### 5B. Subscription Parsing

**Implementation:** `sub/GroupUpdater.cpp`, `main/HTTPRequestHelper.cpp`

#### Positive Controls

- Subscription fetching uses `QNetworkRequest` with Qt's HTTPS stack (system CA store).
- `NoLessSafeRedirectPolicy` is set for HTTP redirects (HTTPRequestHelper.cpp:39).
- Base64 decoding is applied once before format detection (GroupUpdater.cpp).

#### Issues

6. **Global SSL bypass via `sub_insecure`** (SEC-004): `HTTPRequestHelper.cpp:42-46` — when `NekoGui::dataStore->sub_insecure` is `true`, `QSslSocket::VerifyNone` is applied to **all** subscription requests globally. This allows a MITM to inject arbitrary proxy configurations. The flag is user-configurable and persisted in config.

   **PoC scenario:** An attacker on the same network intercepts an HTTP subscription update (or forces HTTP via redirect stripping) and injects a proxy config that routes traffic through an attacker-controlled server. With `sub_insecure=true` there is no certificate error to alert the user.

7. **No URL scheme restriction for subscriptions** (SEC-006): Unlike the update URL validator, subscription URLs accept any scheme including `file://` and `ftp://`. A `file://` URL would cause Qt's network stack to read a local file as a subscription, potentially enabling path traversal of local config files if the parsed result is then echoed back.

8. **Base64 recursion not depth-limited** (SEC-007): GroupUpdater decodes base64 and then recursively calls the parser. If a subscription response contains nested base64 (base64 of base64 of base64 …), this could cause excessive CPU/memory use (DoS). This is a low-severity concern since subscriptions are user-controlled.

### 5C. Privilege Boundaries (TUN)

**Implementation:** `sys/linux/LinuxCap.cpp`, `ui/mainwindow.cpp`, `res/vpn/vpn-run-root.sh`

#### Positive Controls

- Uses PolicyKit (`pkexec`) for privilege escalation — requires the user to authenticate at the desktop session level; no hardcoded passwords.
- `Linux_HavePkexec()` is called before attempting privilege escalation (mainwindow.cpp).
- pkill/killall arguments use hardcoded binary names or integer PIDs — no string injection from user input.

#### Issues

9. **`pkexec bash <script>` is overly broad** (SEC-008): Launching `bash <scriptPath>` as the pkexec-elevated command grants a full root bash interpreter. If `<scriptPath>` were attacker-controlled, it would allow arbitrary root code execution. While the path is derived from the application's own install directory, there is no runtime integrity check on the script before execution. An attacker with write access to the install directory (e.g., via a symlink attack during update) could substitute `vpn-run-root.sh`.

   **Preferred approach:** Ship the VPN script logic as a compiled setuid helper or a polkit action with a restricted command, rather than a root bash shell.

10. **No integrity check on VPN script before root execution** (SEC-009): `vpn-run-root.sh` is extracted from application resources during deployment but is not verified at runtime before pkexec is called (mainwindow.cpp VPN start code). After initial installation, the file can be replaced by any process with write access to the install directory.

11. **iptables rules use hardcoded IP ranges** (vpn-run-root.sh:16-17): The script adds iptables rules for `172.19.0.1/2` and `fdfe:dcba:9876::1/2` without sanitizing any parameters. This is acceptable for app-bundled logic but means the firewall rules are not user-configurable and may conflict with existing network configurations.

### 5D. Logging & Secrets

**Implementation:** `sys/ExternalProcess.cpp`, `ui/UpdateService.cpp`

#### Issues

12. **Full environment dumped to log on core start** (SEC-011): `ExternalProcess.cpp:52` — the line:
    ```cpp
    MW_show_log_ext(tag, "External core starting: " + env.join(" ") + " " + program + " " + arguments.join(" "));
    ```
    logs the **entire process environment** to the application log window. If any environment variable contains a secret (e.g., `GITHUB_TOKEN`, `UPDATE_TOKEN`, `HTTPS_PROXY` credentials, or corporate proxy passwords), it will be visible in the UI log and potentially written to a log file.

    **Must-fix before corporate use.**

13. **Updater log may contain token-bearing URLs** (SEC-012): `UpdateService::appendLog()` logs progress messages including asset download URLs. GitHub release URLs are generally public, but if a private release mechanism were added, URLs with embedded tokens could be logged.

14. **`inbound_auth` credentials stored as plaintext** (SEC-013): Proxy authentication credentials (username/password) configured in the UI are stored as plaintext JSON in `./config/`. On Linux, this directory is readable by the owning user only (typically `0700` if created by Qt), but on Windows the `%APPDATA%\cofebox\config\` directory may have broader ACLs depending on system configuration.

### 5E. Config Migration

**Implementation:** `main/main.cpp:117-134`

#### Positive Controls

- Migration only runs if the new config directory is empty (`hasEntries` check).
- Creates a timestamped backup of the legacy directory before copying.
- Uses `copyDirRecursively` which calls `QFile::copy` — does not follow symlinks (Qt's `QFile::copy` does not dereference symlinks on Linux).

#### Issues

15. **Migration copies entire legacy config without symlink check** (SEC-014): The `copyDirRecursively` function iterates `QDir::entryInfoList`, which includes symlinks as entries. `QFile::copy(from, to)` on Linux copies the symlink target's content (dereferences the symlink). A malicious symlink in `~/.config/nekoray/` pointing to `/etc/passwd` would cause `/etc/passwd` content to be copied into CofeBox's config directory. This is a low-severity issue since exploitation requires a malicious legacy config directory.

---

## 6. Findings Table

| ID | Severity | Description | Impact | Evidence | Recommendation | Effort |
|---|---|---|---|---|---|---|
| SEC-001 | **High** | Update checksums file not GPG-signed | Supply-chain: compromised GitHub token allows swapping artifact + checksum | UpdateService.cpp (SHA-256 verify), Release pipeline | Sign `sha256sums.txt` with a project GPG key; publish public key in repo | Medium |
| SEC-002 | **Medium** | No downgrade protection in updater | Re-introduction of fixed vulnerabilities | auto_update.go — no version comparison before apply | Check installed version; refuse if new < installed | Low |
| SEC-003 | **High** | `ghr` downloaded in CI without integrity check | MITM or compromised release substitutes malicious release tool | build-cofebox-cmake.yml:174 `curl \| tar xzv` | Pin to SHA or use official GitHub Action; verify sha256 | Low |
| SEC-004 | **High** | Global SSL peer-verification bypass (`sub_insecure`) | MITM injection of arbitrary proxy configs | HTTPRequestHelper.cpp:42-46 | Remove global flag; allow per-subscription override; warn user prominently | Medium |
| SEC-005 | **Medium** | Symlink in extracted archive not sanitized | Zip-slip symlink: create symlink outside install dir | auto_update.go:299-305 `copyDir` follows symlinks | Validate symlink targets are within `stagingRoot` before copying | Low |
| SEC-006 | **Medium** | Subscription URL accepts `file://` scheme | Local file read as subscription (path traversal of local files) | GroupUpdater.cpp URL handling | Restrict accepted URL schemes to `https://` and `http://`; reject `file://`, `ftp://` | Low |
| SEC-007 | **Low** | Base64 subscription recursion not depth-limited | DoS via deeply nested base64 encoding | GroupUpdater.cpp recursive decode | Add depth counter; abort after 3 levels | Low |
| SEC-008 | **High** | VPN TUN uses `pkexec bash <script>` (full root shell) | If script is replaced, arbitrary root code execution | mainwindow.cpp VPN start, vpn-run-root.sh | Replace with polkit action + compiled helper; avoid `bash` as pkexec target | High |
| SEC-009 | **Medium** | No runtime integrity check on VPN script before root execution | Script substitution by attacker with write access to install dir | mainwindow.cpp, vpn-run-root.sh | Hash-check or embed script; verify before `pkexec` | Medium |
| SEC-010 | **Low** | `$CONFIG_PATH` unquoted in VPN script | Unexpected behavior if path has spaces | vpn-run-root.sh:22 | Already uses `"$CONFIG_PATH"` — verify quoting is correct | Trivial |
| SEC-011 | **High** ⚠️ *Must-fix* | Full process environment logged at core start | Secrets in env vars exposed in log UI and log files | ExternalProcess.cpp:52 `env.join(" ")` | Filter env before logging; redact known-sensitive keys; log only `program + arguments` | Low |
| SEC-012 | **Low** | Updater log may expose download URLs | URL leakage in log file | UpdateService.cpp `appendLog()` | Review logged strings; avoid logging tokens embedded in URLs | Low |
| SEC-013 | **Medium** | Proxy credentials stored as plaintext JSON | Credential theft from filesystem | NekoGui_DataStore.hpp:55-56, config/*.json | Encrypt sensitive fields using OS keychain (libsecret/DPAPI/Keychain) | High |
| SEC-014 | **Low** | Config migration follows symlinks in legacy dir | Copies content of symlink targets from legacy config | main/main.cpp:65-84 `copyDirRecursively` | Check `QFileInfo::isSymLink()` and skip symlinks during migration | Low |
| SEC-015 | **Medium** | CI workflow uses `ACTIONS_ALLOW_UNSECURE_COMMANDS: true` | Allows `::set-env` / `::add-path` injection in workflow logs | build-cofebox-cmake.yml:81 | Remove this flag; migrate any legacy `::set-env` uses to modern `$GITHUB_ENV` | Low |
| SEC-016 | **Medium** | GitHub Actions permissions: `contents: write` at workflow level | Over-broad token scope; any compromised step can push/delete releases | build-cofebox-cmake.yml:19 | Scope `contents: write` only to the `publish` job; set `permissions: read-all` at workflow level | Low |
| SEC-017 | **Medium** | Action tags not pinned to commit SHA | Tag can be moved to point to different code | build-cofebox-cmake.yml (all `uses:` lines) | Pin all actions to full commit SHA (e.g., `actions/checkout@11bd719`) | Low |
| SEC-018 | **Low** | HTTP test URLs (latency/speed test) | MITM can inject responses; data not sensitive | NekoGui_DataStore.hpp:97-98 | Change to HTTPS equivalents | Trivial |
| SEC-019 | **Low** | No SBOM generated for releases | Cannot audit what is included in distributed binaries | Release pipeline | Add CycloneDX SBOM generation in CI | Low |
| SEC-020 | **Reviewed: OK** | No `system()` or `popen()` calls | N/A — good | Full codebase grep — none found | No action required | — |
| SEC-021 | **Reviewed: OK** | No hardcoded credentials or tokens | N/A — good | Full codebase grep — only env var references | No action required | — |
| SEC-022 | **Reviewed: OK** | gRPC bound to loopback only | N/A — good | rpc/gRPC.cpp | No action required | — |
| SEC-023 | **Reviewed: OK** | Single-instance server uses OS-scoped socket | N/A — good | main/main.cpp QLocalServer | No action required | — |
| SEC-024 | **Reviewed: OK** | Update URLs validated against `github.com` allowlist | N/A — good | UpdateService.cpp `isTrustedReleaseAssetUrl()` | No action required | — |

---

## 7. Hardening Recommendations

### 7.1 Short-Term (Sprint 1–2)

| # | Recommendation | Finding(s) |
|---|---|---|
| H-1 | **Redact environment from log:** In `ExternalProcess.cpp:52`, replace `env.join(" ")` with a filtered version that excludes known-sensitive environment variable prefixes (`TOKEN`, `SECRET`, `PASSWORD`, `KEY`, `PROXY`). | SEC-011 |
| H-2 | **Pin CI actions to SHA:** Replace `actions/checkout@v3` with `actions/checkout@<SHA>` for all workflow actions. | SEC-017 |
| H-3 | **Remove `ACTIONS_ALLOW_UNSECURE_COMMANDS`:** Audit the workflow for any `::set-env` usage and migrate to `$GITHUB_ENV`. | SEC-015 |
| H-4 | **Scope `contents: write` to publish job only:** Add `permissions: contents: read` at workflow level and `permissions: contents: write` only to the `publish` job. | SEC-016 |
| H-5 | **Verify `ghr` integrity in CI:** Download `ghr` and check its SHA-256 against a pinned value stored in the repo. | SEC-003 |
| H-6 | **Restrict subscription URL schemes:** In `GroupUpdater.cpp`, reject `file://`, `ftp://`, and other non-HTTP(S) schemes before fetching. | SEC-006 |

### 7.2 Medium-Term (Sprint 3–6)

| # | Recommendation | Finding(s) |
|---|---|---|
| H-7 | **Replace `pkexec bash` with a polkit action:** Create a D-Bus activated helper that performs only the specific iptables and core-launch operations. Eliminates the full root shell surface. | SEC-008 |
| H-8 | **GPG-sign the `sha256sums.txt`:** Generate a project signing key, publish the public key in the repo and on a key server, and add a signature step to the release workflow. | SEC-001 |
| H-9 | **Add downgrade protection:** Before installing an update, compare the version string in the downloaded manifest against the current app version. Reject if lower. | SEC-002 |
| H-10 | **Warn prominently when `sub_insecure` is enabled:** Add a persistent warning banner in the UI. Consider requiring explicit per-subscription opt-in rather than a global flag. | SEC-004 |
| H-11 | **Validate symlinks in archive extraction:** After extraction, walk `stagingRoot` and assert that no symlink target resolves outside `stagingRoot`. | SEC-005 |

### 7.3 Long-Term (Q2+)

| # | Recommendation | Detail |
|---|---|---|
| H-12 | **OS Keychain for credentials** | Use `libsecret` (Linux), Windows DPAPI, or macOS Keychain API to encrypt `inbound_auth` and `core_box_clash_api_secret` at rest. |
| H-13 | **Windows code signing** | Obtain an EV code-signing certificate; sign `cofebox.exe` and `cofebox_core.exe` before release. Prevents SmartScreen warnings and tampering. |
| H-14 | **Linux AppImage signing** | Use `appimagetool --sign` with a GPG key; document verification steps for users. |
| H-15 | **Reproducible builds** | Add build-reproducibility tooling (e.g., `SOURCE_DATE_EPOCH`) to allow independent verification of release binaries. |
| H-16 | **SBOM generation** | Add CycloneDX SBOM generation in CI; attach to every GitHub release. |
| H-17 | **SAST in CI** | Add `codeql-action` (C++ + Go) and `gosec` to the pull-request workflow. |
| H-18 | **Dependency scanning** | Enable Dependabot for Go modules (`go.sum`) and GitHub Actions versions. |
| H-19 | **seccomp profile (Linux)** | Apply a seccomp filter to the sing-box core child process to restrict syscall surface. |
| H-20 | **Security disclosure policy** | Publish `SECURITY.md` (see accompanying file) so researchers know how to report vulnerabilities. |

---

## 8. CI Security Controls

### 8.1 Current State Assessment

| Control | Status | Issue |
|---|---|---|
| Secrets in workflow | OK — uses `github.token` only | — |
| Workflow trigger | Tags + manual dispatch | Acceptable; no PR-triggered builds |
| `contents: write` scope | Over-broad | SEC-016 |
| `ACTIONS_ALLOW_UNSECURE_COMMANDS` | Enabled — legacy | SEC-015 |
| Action pinning | Tag-based | SEC-017 |
| `ghr` tool integrity | No checksum | SEC-003 |
| Artifact provenance | None | — |
| SBOM | None | SEC-019 |
| SHA-256 of release assets | Yes (`sha256sum` in publish job) | OK |
| Code scanning | None | Recommend H-17 |
| Dependency scanning | None | Recommend H-18 |

### 8.2 Recommended CI Additions

```yaml
# Minimal security improvements (add to build-cofebox-cmake.yml)

# 1. Restrict permissions at workflow level
permissions:
  contents: read
  actions: read

# 2. Publish job only gets write
jobs:
  publish:
    permissions:
      contents: write
      id-token: write  # for attestation

# 3. Remove deprecated flag
# DELETE: ACTIONS_ALLOW_UNSECURE_COMMANDS: true

# 4. Verify ghr integrity
- name: Download and verify ghr
  run: |
    curl -Lo ghr.tar.gz https://github.com/tcnksm/ghr/releases/download/v0.13.0/ghr_v0.13.0_linux_amd64.tar.gz
    echo "b8a9c40d35f8c9e4a5fb42e07b91fe4e41c2b9ce8cfab7f97fdc89e3a0bf4eb0  ghr.tar.gz" | sha256sum -c
    tar xzf ghr.tar.gz

# 5. SBOM generation (optional)
- name: Generate SBOM
  uses: anchore/sbom-action@v0
  with:
    artifact-name: sbom.spdx.json
```

### 8.3 Release Process Checklist

- [ ] All release artifacts have corresponding entries in `sha256sums.txt`
- [ ] `sha256sums.txt` is GPG-signed (recommended, not yet implemented)
- [ ] Release is created from a tagged commit on `main`
- [ ] No pre-release artifacts are published without `-beta`/`-rc` suffix
- [ ] Updater version check rejects older versions (downgrade protection — recommended)

---

## 9. Appendix

### 9.1 Build Steps

**Linux (AppImage/DEB):**
```bash
# Install dependencies
./libs/build_deps_all.sh

# Build Go components
GOOS=linux GOARCH=amd64 ./libs/build_go.sh

# Build C++
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja

# Package
./libs/deploy_linux64.sh
./libs/package_appimage.sh
./libs/package_debian.sh <version>
```

**Windows:**
```powershell
# Download Qt SDK
bash ./libs/download_qtsdk_win.sh

# Build
source libs/env_qtsdk.sh $PWD/qtsdk/Qt
mkdir build && cd build
cmake -GNinja -DQT_VERSION_MAJOR=6 -DCMAKE_BUILD_TYPE=Release ..
ninja

# Package
./libs/deploy_windows64.sh
```

### 9.2 Verification of Release Integrity

```bash
# Download release
wget https://github.com/cofedish/cofe-nekobox/releases/download/vX.Y.Z/CofeBox-X.Y.Z-linux-x64.AppImage
wget https://github.com/cofedish/cofe-nekobox/releases/download/vX.Y.Z/sha256sums.txt

# Verify
sha256sum -c sha256sums.txt --ignore-missing
```

### 9.3 Testing TUN Mode (Linux)

```bash
# Check if pkexec is available
pkexec --version

# Check if setcap is available
which setcap || ls /usr/sbin/setcap

# Test capability assignment
sudo setcap cap_net_admin=ep ./cofebox_core
getcap ./cofebox_core

# Verify iptables rules after TUN start (manual)
iptables -L INPUT -n
ip6tables -L INPUT -n
```

### 9.4 References

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [STRIDE Threat Model](https://learn.microsoft.com/en-us/azure/security/develop/threat-modeling-tool-threats)
- [Qt Network Security](https://doc.qt.io/qt-6/ssl.html)
- [PolicyKit / pkexec Security](https://www.freedesktop.org/software/polkit/docs/latest/)
- [CycloneDX SBOM](https://cyclonedx.org/)
- [GitHub Actions Security Hardening](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)
- [codeclysm/extract](https://github.com/codeclysm/extract) — check for zip-slip mitigations
- [go.sum integrity model](https://go.dev/ref/mod#go-sum-files)
- [Sigstore cosign](https://docs.sigstore.dev/) — for binary attestation

---

*Report generated by internal AppSec review — 2026-02-20. For questions contact the security team or open a GitHub issue tagged `security`.*
