# CofeBox

CofeBox — форк NekoRay/NekoBox: кроссплатформенный Qt‑GUI для sing-box. Проект сохраняет совместимость конфигов и фокусируется на современном, минималистичном UI.

## Возможности
- Подписки и импорты (URL/буфер обмена)
- Быстрое подключение/отключение
- Профили, группы, правила и логи
- Темы: System / Light / Dark / Lucifer
- Reduce motion для отключения анимаций

## Установка
Релизы находятся в репозитории проекта:
- Windows: ZIP из Releases
- Linux: AppImage или deb

Скачать: https://github.com/cofedish/cofe-nekobox/releases

## Быстрый старт
1) На Home вставьте URL подписки и нажмите «Добавить».
2) Нажмите «Подключиться» в центре экрана.
3) При необходимости включите системный прокси или TUN в настройках.

## Документация
- `docs/INSTALL.md` — установка и обновление
- `docs/USAGE.md` — базовые сценарии
- `docs/THEMES.md` — темы и reduce motion
- `docs/ASSETS.md` — иконки и ресурсы

## Сборка из исходников
Требования (минимально):
- Qt 5.12–5.15 (Widgets, Network, Svg, LinguistTools)
- CMake + Ninja
- Компилятор C++17

Команды:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Подробности по платформам см. в `docs/`.

## Ссылки
- Репозиторий: https://github.com/cofedish/cofe-nekobox
- Issues: https://github.com/cofedish/cofe-nekobox/issues
- Releases: https://github.com/cofedish/cofe-nekobox/releases
