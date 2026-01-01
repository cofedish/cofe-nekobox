# Ресурсы и иконки

## Где лежат ресурсы
- `res/cofebox.ico` — иконка приложения для Windows (вшивается в `.exe`).
- `res/public/cofebox.png` — базовая PNG‑иконка (окно/трей/упаковка).
- `res/neko.qrc` — QRC с публичными ресурсами (включает `cofebox.png`).
- `res/icon/*.svg` и `res/icon/material/*.svg` — SVG‑иконки для кнопок/меню.
- `res/theme/**` — картинки для старых QSS‑тем (если используете).

## Замена иконки приложения
### Windows
1) Замените `res/cofebox.ico` (мульти‑размер `.ico`).
2) Убедитесь, что `cmake/windows/windows.cmake` указывает на `res/cofebox.ico`.
3) Пересоберите проект (Release).
4) Если иконка не обновилась — очистите кэш иконок Windows:
   - `ie4uinit.exe -ClearIconCache`
   - перезапустите Explorer или ПК.

### Linux (Debian/AppImage)
1) Замените `res/public/cofebox.png` (квадратная PNG).
2) Debian упаковка использует `libs/package_debian.sh`.
3) AppImage использует `libs/package_appimage.sh`.
4) В CI ресурсы копируются через `libs/build_public_res.sh`.

### macOS
Готовой упаковки нет. При добавлении:
- положите `res/cofebox.icns` и подключите в CMake/Info.plist.

## Иконка трея
- Логика в `ui/Icon.cpp`: берётся `:/neko/cofebox.png`, поверх рисуется статус.
- Для замены обновите `res/public/cofebox.png` и `res/neko.qrc`, затем пересоберите.

## Логотип на странице «О программе»
- Используется тот же ресурс `:/neko/cofebox.png`.
- Отображение настраивается в `ui/mainwindow.ui` (виджет `about_logo`) и в `ui/mainwindow.cpp`.
- Рекомендуемые размеры исходника: 256x256 или 512x512 PNG.

## Иконки меню/кнопок
- SVG находятся в `res/icon/*.svg` и `res/icon/material/*.svg`.
- Используются через `Icon::GetMaterialIcon("имя")` в `ui/Icon.cpp`.
- Рекомендация: единый стиль stroke/size, лучше SVG для HiDPI.

## Размеры и правила
- `.ico`: 16/20/24/32/48/64/128/256 px.
- PNG: квадрат 256x256 или 512x512.
- Для HiDPI: SVG или 2x PNG.

## Проверка после замены
- Иконки окна и трея должны обновиться.
- В zip/deb/AppImage должна быть актуальная `cofebox.png`.
- На Windows при проблемах увеличьте версию или очистите кэш.
