# QA Smoke Checklist

## 1) Subscription proxy

1. Open `Servers` page.
2. Add subscription URL and click `Add`.
3. Verify new profiles appear in the target group.
4. Select one profile from that subscription and start it.
5. Enable `System Proxy` (or `Tun` if needed).
6. Verify traffic goes through selected profile.

Quick check with SOCKS:

```bash
curl --proxy socks5h://127.0.0.1:2080 https://ifconfig.io
```

## 2) Whitelist / bypass / direct rules

1. Open routing settings and add `example.com` to direct/bypass domain list.
2. Apply routing and restart profile.
3. Verify `example.com` is routed to direct/bypass, other domains still use proxy.

Quick checks:

```bash
curl --proxy socks5h://127.0.0.1:2080 https://example.com
curl --proxy socks5h://127.0.0.1:2080 https://ifconfig.io
```

For process/cidr user rules in VPN settings:

- add process names or CIDR in `VPN settings`
- enable TUN
- verify generated route rules are applied.

## 3) TUN mode (Linux)

1. Enable TUN in CofeBox.
2. Confirm no permission/setup errors in logs.
3. Check TUN interface exists and routes are applied.

Commands:

```bash
ip a | grep -E "neko-tun|utun|tun"
ip route
curl https://ifconfig.io
```

If AppImage asks one-time elevation, accept it and wait for automatic restart.
