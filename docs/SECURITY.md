# CofeBox Security Policy

## Supported Versions

| Version | Supported |
|---|---|
| Latest release on `main` | ✅ Yes |
| Older releases | ⚠️ Best-effort |

We strongly recommend always using the latest published release.

---

## Reporting a Vulnerability

**Please do NOT report security vulnerabilities through public GitHub Issues.**

### How to Report

1. **Preferred:** Open a [GitHub Security Advisory](https://github.com/cofedish/cofe-nekobox/security/advisories/new) (private, GitHub-hosted disclosure).

2. **Alternative:** Send an email to the maintainer. Check the GitHub profile of the repository owner for a contact address.

3. Include in your report:
   - A description of the vulnerability and its potential impact
   - Steps to reproduce or a proof-of-concept (do not include weaponized exploits)
   - Affected version(s) / commit(s)
   - Any relevant log output or screenshots (with sensitive data redacted)

### What to Expect

| Timeline | Action |
|---|---|
| **48 hours** | Acknowledgement of receipt |
| **7 days** | Initial severity assessment and response |
| **30 days** | Target for patch release (Critical/High findings) |
| **90 days** | Maximum window before coordinated public disclosure |

We follow a **coordinated disclosure** model. We ask that you give us reasonable time to address the issue before publishing. We will credit you in the release notes (unless you prefer to remain anonymous).

---

## Security Scope

### In Scope

- CofeBox application code (C++/Qt, Go)
- Auto-update mechanism (UpdateService, updater binary)
- Privilege escalation flows (pkexec, TUN mode)
- Subscription import and parsing
- Configuration storage and migration
- CI/CD pipeline security
- Distribution packages (ZIP, AppImage, DEB)

### Out of Scope

- Security of upstream [sing-box](https://github.com/SagerNet/sing-box) core
- Security of third-party subscription providers
- Vulnerabilities in the Qt framework itself (report to Qt Project)
- Attacks requiring physical access to the device
- Social engineering attacks

---

## Security Hardening Notes

### For Users

- **Do not enable `sub_insecure` (Allow Insecure Connections)** unless you fully trust your network and subscription provider. This flag disables SSL certificate validation for subscription fetching.
- Store your CofeBox config directory with restrictive permissions (`chmod 700 ~/.config/cofebox`).
- Verify release checksums before installing updates manually (see [docs/SECURITY_AUDIT.md](SECURITY_AUDIT.md) §9.2).
- Review the subscriptions you import — only import from trusted sources.

### For Corporate Deployments

- Read the full [Security Audit Report](SECURITY_AUDIT.md) before deployment.
- Consider disabling the auto-update feature and managing updates through a controlled internal distribution channel.
- Apply the `Must-fix before corporate use` items identified in the audit (SEC-011 in particular).
- Ensure the install directory has restrictive filesystem permissions so that only the application user can write to it.

---

## Known Security Limitations

The following limitations are acknowledged and documented in the [Security Audit Report](SECURITY_AUDIT.md):

| ID | Description | Workaround |
|---|---|---|
| SEC-001 | SHA-256 checksums are not GPG-signed | Verify download is from the official GitHub release page |
| SEC-004 | `sub_insecure` allows global SSL bypass | Keep this setting disabled |
| SEC-008 | TUN mode uses `pkexec bash <script>` | Only enable TUN mode on trusted systems |
| SEC-011 | Process environment may be logged at core start | Avoid setting sensitive env vars in the shell that launches CofeBox |
| SEC-013 | Proxy credentials stored as plaintext JSON | Protect config directory with filesystem permissions |

---

## PGP / Contact

For sensitive disclosures, check the repository owner's GitHub profile for PGP key information, or use GitHub's private security advisory feature which provides end-to-end encrypted communication.

---

*This policy was established following an internal security audit on 2026-02-20.*
