# QA Smoke Plan (Security + Routing)

## 1. Subscription Update Flow

1. Open `Settings -> Basic Settings -> Subscription` and keep `Use proxy when updating subscription` as needed.
2. Add a valid `https://...` subscription URL from `Home` (`Paste subscription URL`) or `Servers`.
3. Confirm profiles are imported (`Imported N profile(s)` / new profiles in current group).
4. For saved subscription groups, open `Manage groups -> Edit` and verify:
   - `Insecure TLS for this subscription` is **OFF** by default.
   - Warning text appears only when it is enabled.

Expected:
- `http/https` URLs are accepted.
- `file://`, `ftp://`, `data:`, `javascript:` are rejected with `Only http(s) URLs are allowed.`

## 2. Proxy By Subscription (Regression)

1. Pick a profile imported from subscription.
2. Start proxy.
3. Verify traffic through local mixed port:

```bash
curl --proxy http://127.0.0.1:2080 https://api.ipify.org
```

4. Switch to another profile from another subscription and repeat.

Expected:
- Active profile switch changes outbound route behavior.
- No regression in selecting/running subscription-derived profiles.

## 3. Whitelist / Bypass / Direct Rules (Regression)

1. Add a direct/bypass rule for `example.com` in routing rules.
2. Keep another domain (for example `ifconfig.me`) through proxy.
3. Validate:

```bash
curl --proxy http://127.0.0.1:2080 https://example.com -I
curl --proxy http://127.0.0.1:2080 https://ifconfig.me
```

Expected:
- `example.com` follows direct/bypass route.
- Other domains still use proxy route.

## 4. Linux TUN Smoke

1. Enable TUN mode.
2. Validate TUN device and routes:

```bash
ip a | grep -E "tun|cofebox"
ip route
```

Expected:
- TUN process starts with privilege prompt.
- If `/dev/net/tun` is missing, UI shows explicit error.

## 5. Secret Redaction Smoke

1. Launch app from terminal with a fake secret env var:

```bash
export GITHUB_TOKEN=SHOULD_NOT_APPEAR_IN_LOGS
./cofebox
```

2. Start/stop proxy once.
3. Check logs/UI log panel for raw token value.

Expected:
- Token value is never logged.
- Startup log contains only env summary/count, not full env values.

