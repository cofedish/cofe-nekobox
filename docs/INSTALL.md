# Установка и обновление

## Windows
1) Скачайте ZIP из Releases.
2) Распакуйте в отдельную папку.
3) Запустите `CofeBox.exe` (если файл называется иначе — используйте exe из архива).

Обновление: скачайте новый ZIP и замените содержимое папки приложения.

## Linux
### AppImage
1) Скачайте `.AppImage` из Releases.
2) Дайте права на запуск: `chmod +x CofeBox-*.AppImage`.
3) Запустите файл.

### Debian/Ubuntu (.deb)
1) Скачайте `.deb` из Releases.
2) Установите: `sudo dpkg -i cofebox-*-debian-x64.deb`.
3) При необходимости выполните `sudo apt -f install`.

## Где хранятся настройки
По умолчанию приложение хранит конфиги в каталоге `config` рядом с исполняемым файлом.
В режиме AppData путь зависит от системы:
- Linux: `~/.config/cofebox`
- Windows: `%APPDATA%/cofebox`
