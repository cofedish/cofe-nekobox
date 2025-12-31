# ICONS_AND_ASSETS

## Где лежат ресурсы
- `res/cofebox.ico` — иконка приложения для Windows (вшивается в .exe через CMake).
- `res/public/cofebox.png` — базовая PNG-иконка (окно/трей/упаковка).
- `res/neko.qrc` — QRC с публичными ресурсами (включает `cofebox.png`).
- `res/icon/*.svg` и `res/icon/material/*.svg` — SVG-иконки для кнопок/меню.
- `res/theme/**` — картинки для старых тем QSS (если используешь их, меняй здесь).

## Замена иконки приложения
### Windows
1) Замени файл `res/cofebox.ico` на свой (мульти-ресайз .ico).
2) Проверь, что `cmake/windows/windows.cmake` указывает на `res/cofebox.ico`.
3) Пересобери проект (Release).
4) Если иконка не обновилась — увеличь версию (tag) и очисти кэш иконок Windows:
   - `ie4uinit.exe -ClearIconCache`
   - перезапусти проводник (Explorer) или ПК.

### Linux (Debian/AppImage)
1) Замени `res/public/cofebox.png` (квадратная PNG).
2) Упаковка Debian: `libs/package_debian.sh` использует `Icon=/opt/nekoray/cofebox.png`.
3) AppImage: `libs/package_appimage.sh` использует `Icon=cofebox`.
4) В CI ресурсы копируются из `res/public` через `libs/build_public_res.sh`.

### macOS
В проекте нет готовой упаковки. Если добавишь:
- положи `res/cofebox.icns` и подключи его в CMake/Info.plist для bundle.

## Иконка трея
- Логика в `ui/Icon.cpp`: берется `:/neko/cofebox.png`, затем поверх рисуется статус.
- Замена: обнови `res/public/cofebox.png` и `res/neko.qrc`, пересобери.

## Иконки меню/кнопок
- SVG лежат в `res/icon/*.svg` и `res/icon/material/*.svg`.
- Используются через `Icon::GetMaterialIcon("имя_файла")` в `ui/Icon.cpp`.
- Рекомендация: сохраняй единый стиль (stroke/size), лучше SVG для HiDPI.

## Размеры и правила
- `.ico`: 16/20/24/32/48/64/128/256 px.
- PNG: квадрат 256x256 или 512x512.
- Для HiDPI: SVG или 2x PNG.

## Проверка после замены
- Запусти приложение: иконки окна и трея должны обновиться.
- Проверь пакеты (zip/deb/AppImage) — внутри должна быть `cofebox.png`.
- На Windows при проблемах очисть кэш иконок или увеличь версию.
