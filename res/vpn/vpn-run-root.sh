#!/bin/sh
set -eu

PATH="/usr/sbin:/usr/bin:/sbin:/bin"
export PATH

if [ "${EUID:-$(id -u)}" -ne 0 ]; then
  echo "[Error] Tun script must run as root"
  exit 1
fi

CORE_PATH="${1:-}"
CONFIG_PATH="${2:-}"

if [ -z "$CORE_PATH" ] || [ -z "$CONFIG_PATH" ]; then
  echo "[Error] Usage: vpn-run-root.sh <core-path> <config-path>"
  exit 2
fi

if [ ! -e "/dev/net/tun" ]; then
  echo "[Error] /dev/net/tun is missing"
  exit 3
fi

pre_start_linux() {
  # for Tun2Socket
  iptables -I INPUT -s 172.19.0.2 -d 172.19.0.1 -p tcp -j ACCEPT
  ip6tables -I INPUT -s fdfe:dcba:9876::2 -d fdfe:dcba:9876::1 -p tcp -j ACCEPT
}

start() {
  pre_start_linux
  "$CORE_PATH" run -c "$CONFIG_PATH"
}

stop() {
  iptables -D INPUT -s 172.19.0.2 -d 172.19.0.1 -p tcp -j ACCEPT || true
  ip6tables -D INPUT -s fdfe:dcba:9876::2 -d fdfe:dcba:9876::1 -p tcp -j ACCEPT || true
}

start || true
stop
