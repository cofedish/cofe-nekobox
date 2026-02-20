# CofeBox Security Baseline Checklist

This checklist is derived from the [Security Audit Report](SECURITY_AUDIT.md) (2026-02-20).
Use it to track remediation progress before production/corporate deployment.

**Legend:** ✅ Done / Compliant | ⚠️ Partial | ❌ Not done | 📋 Reviewed OK (no action needed)

---

## A. Must-Fix Before Corporate Use

| # | Check | Status | Finding | Owner |
|---|---|---|---|---|
| A-1 | Environment variables are **NOT** logged verbatim at core startup | ❌ | SEC-011 (`ExternalProcess.cpp:52`) | Dev |
| A-2 | `sub_insecure` flag has a prominent UI warning when enabled | ❌ | SEC-004 | Dev |
| A-3 | Subscription URL scheme is restricted to `http://` and `https://` only | ❌ | SEC-006 | Dev |
| A-4 | CI does not use `ACTIONS_ALLOW_UNSECURE_COMMANDS: true` | ❌ | SEC-015 | DevOps |
| A-5 | `ghr` download in CI is integrity-checked (SHA-256) | ❌ | SEC-003 | DevOps |

---

## B. Network Security

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| B-1 | All update downloads use HTTPS | ✅ | SEC-024 | UpdateService `isTrustedReleaseAssetUrl()` |
| B-2 | Update URLs are restricted to `github.com` domain | ✅ | SEC-024 | Allowlist in UpdateService |
| B-3 | Downloaded update artifacts are SHA-256 verified before execution | ✅ | — | `verifySha256File()` |
| B-4 | GitHub API calls use HTTPS | ✅ | — | AppInfo.hpp endpoint |
| B-5 | Subscription fetching uses TLS by default | ✅ | — | Qt HTTPS |
| B-6 | SSL bypass (`sub_insecure`) is **disabled** by default | ✅ | SEC-004 | Default `false` |
| B-7 | SSL bypass requires explicit per-session user confirmation | ❌ | SEC-004 | Currently a persistent setting |
| B-8 | Latency/speed test URLs use HTTPS | ❌ | SEC-018 | `http://cp.cloudflare.com/` |
| B-9 | No certificate pinning for GitHub update API | ⚠️ | SEC-001 | Acceptable for non-critical path |
| B-10 | gRPC core bound to loopback only | ✅ | SEC-022 | `127.0.0.1` |

---

## C. Privilege & Process Execution

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| C-1 | No `system()` or `popen()` calls | ✅ | SEC-020 | Grep confirmed zero occurrences |
| C-2 | All process spawning uses `QProcess` (no shell injection path) | ✅ | — | Full codebase review |
| C-3 | pkexec used for privilege escalation (not setuid binary) | ✅ | — | PolicyKit integration |
| C-4 | TUN mode does NOT use `pkexec bash` (full shell) | ❌ | SEC-008 | `mainwindow.cpp` VPN start |
| C-5 | VPN script integrity is verified before root execution | ❌ | SEC-009 | No hash check at runtime |
| C-6 | setcap arguments come from app internals, not user input | ✅ | — | `LinuxCap.cpp` |
| C-7 | Process arguments are not constructed from unvalidated user input | ✅ | — | PID as int, hardcoded names |
| C-8 | DEB install via pkexec uses SHA-256 verified package | ✅ | — | UpdateService verifies before calling |
| C-9 | Updater binary launched from absolute or application-relative path | ✅ | — | Relative to app dir |

---

## D. Filesystem & Config

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| D-1 | Config directory created with user-only permissions | ⚠️ | SEC-013 | Qt default; verify on Windows |
| D-2 | Proxy credentials encrypted at rest (OS keychain) | ❌ | SEC-013 | Plaintext JSON |
| D-3 | Symlinks in extracted archives are validated (zip-slip protection) | ❌ | SEC-005 | `copyDir` in auto_update.go |
| D-4 | Config migration does not follow malicious symlinks | ⚠️ | SEC-014 | `QFile::copy` dereferences symlinks |
| D-5 | Temp directory for update staging is private (non-world-writable) | ⚠️ | — | `os.TempDir()` — system-dependent |
| D-6 | Old temp files are cleaned up after update | ✅ | — | `defer os.RemoveAll(stagingRoot)` |
| D-7 | Backup created before overwriting installation | ✅ | — | auto_update.go `copyDir` to backup |
| D-8 | Rollback attempted on installation failure | ✅ | — | auto_update.go:134 |

---

## E. Logging & Secrets

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| E-1 | Process environment NOT logged verbatim | ❌ | SEC-011 | `ExternalProcess.cpp:52` — CRITICAL |
| E-2 | No hardcoded credentials in codebase | ✅ | SEC-021 | Grep confirmed — only env var refs |
| E-3 | `core_token` not persisted to disk | ✅ | — | Runtime-only, written to stdin |
| E-4 | GitHub token loaded from env var, not hardcoded | ✅ | — | `UPDATE_TOKEN` / `GITHUB_TOKEN` |
| E-5 | Log files do not contain auth tokens | ⚠️ | SEC-012 | Review updater.log content |
| E-6 | Sensitive fields redacted before display in UI | ⚠️ | SEC-011 | Partial — no formal redaction policy |

---

## F. Supply Chain & CI/CD

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| F-1 | GitHub Actions permissions scoped to minimum required | ❌ | SEC-016 | `contents: write` too broad |
| F-2 | All GitHub Actions pinned to commit SHA | ❌ | SEC-017 | Tag-based only |
| F-3 | `ACTIONS_ALLOW_UNSECURE_COMMANDS` removed | ❌ | SEC-015 | Line 81 of workflow |
| F-4 | CI tool downloads verified with checksums | ❌ | SEC-003 | `ghr` not verified |
| F-5 | Release artifacts have SHA-256 checksums | ✅ | — | `sha256sum` in publish job |
| F-6 | Release checksums are GPG-signed | ❌ | SEC-001 | Not implemented |
| F-7 | SBOM generated for each release | ❌ | SEC-019 | Not implemented |
| F-8 | Dependabot enabled for Go modules | ❌ | — | No `.github/dependabot.yml` |
| F-9 | SAST (CodeQL or equivalent) runs on PRs | ❌ | — | No SAST workflow |
| F-10 | Docker base image pinned to digest | ⚠️ | — | Date-tagged, not SHA |

---

## G. Distribution Packages

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| G-1 | Windows binary is code-signed | ❌ | — | No signing in CI |
| G-2 | Linux AppImage is GPG-signed | ❌ | — | No signing |
| G-3 | DEB package integrity verified by APT (signed repo) | ❌ | — | Direct .deb install via pkexec |
| G-4 | SHA-256 checksums published with every release | ✅ | — | `sha256sums.txt` in release |
| G-5 | Users instructed how to verify checksums | ❌ | — | No docs yet (add to README) |

---

## H. Security Policy & Process

| # | Check | Status | Finding | Notes |
|---|---|---|---|---|
| H-1 | `SECURITY.md` published at repo root / `.github/` | ✅ | — | Added in this audit |
| H-2 | Private vulnerability reporting enabled (GitHub Security Advisories) | ❌ | — | Must enable in GitHub repo settings |
| H-3 | Response SLA defined and published | ✅ | — | `docs/SECURITY.md` |
| H-4 | Security audit conducted and documented | ✅ | — | This document |
| H-5 | Next scheduled audit date defined | ❌ | — | Recommend 12-month cycle |

---

## Compliance Summary

| Category | Total Checks | Passed (✅/📋) | Partial (⚠️) | Failed (❌) |
|---|---|---|---|---|
| A. Must-Fix | 5 | 0 | 0 | 5 |
| B. Network | 10 | 7 | 1 | 2 |
| C. Privilege | 9 | 7 | 0 | 2 |
| D. Filesystem | 8 | 3 | 3 | 2 |
| E. Logging | 6 | 3 | 2 | 1 |
| F. Supply Chain | 10 | 2 | 2 | 6 |
| G. Distribution | 5 | 1 | 0 | 4 |
| H. Policy | 5 | 3 | 0 | 2 |
| **Total** | **58** | **26 (45%)** | **8 (14%)** | **24 (41%)** |

**Recommendation:** Resolve all Category A items and at least 50% of Category F items before recommending CofeBox for corporate deployment.

---

*Last updated: 2026-02-20 by Security Audit (commit docs: add security audit report + policy + checklist)*
