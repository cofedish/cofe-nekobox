# Linux TUN (deb and AppImage)

## Как работает TUN в CofeBox

- Для TUN используется `nekobox_core` (sing-box runtime).
- Для создания TUN-интерфейса нужны повышенные сетевые права:
  - `cap_net_admin`
  - `cap_net_bind_service` (добавлено вместе с `cap_net_admin`).

## Почему AppImage отличается от deb

- В `deb` пакет ставится в обычную файловую систему, поэтому `setcap` на `/opt/nekoray/nekobox_core` сохраняется.
- В `AppImage` бинарь находится внутри read-only mount, и постоянный `setcap` там невозможен.

## Что делает приложение в AppImage (one-time flow)

При первом включении TUN:

1. CofeBox извлекает `nekobox_core` в writable-путь:
   - `~/.local/share/cofebox/bin/nekobox_core`
2. Проверяет бинарь и переиспользует его, если версия/хеш не изменились.
3. Проверяет:
   - `/dev/net/tun`
   - `setcap`
   - `pkexec`
4. Запрашивает one-time elevation и выполняет:
   - `setcap cap_net_admin,cap_net_bind_service=ep ~/.local/share/cofebox/bin/nekobox_core`
5. После успеха перезапускает CofeBox, и TUN начинает работать обычным пользователем.

## Ошибки и что делать

- `/dev/net/tun does not exist`:
  - проверьте, что модуль TUN доступен в системе.
- `pkexec` отсутствует:
  - Debian/Ubuntu: `sudo apt install policykit-1`
- `setcap` отсутствует:
  - Debian/Ubuntu: `sudo apt install libcap2-bin`

## Как откатить helper

Удалите извлечённый helper:

```bash
rm -f ~/.local/share/cofebox/bin/nekobox_core
```

При следующем включении TUN CofeBox заново подготовит helper и попросит one-time elevation.
