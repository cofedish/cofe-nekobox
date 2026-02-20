ењЁ Windows дё‹зј–иЇ‘ CofeBox

### git clone жєђз Ѓ

```
git clone <URL_ЭТОГО_РЕПОЗИТОРИЯ> --recursive
```

### е®‰иЈ… Visual Studio

д»Ћеѕ®иЅЇе®зЅ‘е®‰иЈ…пјЊеЏЇд»ҐдЅїз”Ё 2019 е’Њ 2022 з‰€жњ¬пјЊе®‰иЈ… Win32 C++ ејЂеЏ‘зЋЇеўѓгЂ‚

е®‰иЈ…еҐЅеђЋеЏЇд»ҐењЁгЂЊејЂе§‹гЂЌиЏњеЌ•ж‰ѕе€° `x64 Native Tools Command Prompt`

жњ¬ж–‡д№‹еђЋзљ„е‘Ѕд»¤еќ‡ењЁиЇҐ cmd е†…ж‰§иЎЊгЂ‚`cmake` `ninja` з­‰е·Ґе…·дЅїз”Ё VS и‡Єеё¦зљ„еЌіеЏЇгЂ‚

### дё‹иЅЅ Qt SDK

з›®е‰Ќ Windows Release дЅїз”Ёзљ„з‰€жњ¬жЇ Qt 6.5.x

дё‹иЅЅи§ЈеЋ‹еђЋпјЊе°† bin з›®еЅ•ж·»еЉ е€°зЋЇеўѓеЏй‡ЏгЂ‚

#### Release зј–иЇ‘з”Ёе€°зљ„ Qt еЊ…дё‹иЅЅ (MSVC2019 x86_64)

https://download.qt.io/official_releases/qt/6.5/

#### е®ж–№з­ѕеђЌз‰€ Qt 5.15.2 пј€еЏЇйЂ‰пјЊе·ІзџҐжњ‰е†…е­жі„жјЏзљ„BUGпј‰

ењЁж­¤дё‹иЅЅ `qtbase` `qtsvg` `qttools` зљ„еЊ…е№¶и§ЈеЋ‹е€°еђЊдёЂдёЄз›®еЅ•гЂ‚

https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt5_5152/qt.qt5.5152.win64_msvc2019_64/

### C++ йѓЁе€†зј–иЇ‘

#### зј–иЇ‘е®‰иЈ… C/C++ дѕќиµ–

пј€иї™дёЂж­ҐеЏЇиѓЅи¦ЃжЊ‚жўЇпј‰

```shell
bash ./libs/build_deps_all.sh
```

з›®е‰ЌеЏЄжњ‰ bash и„љжњ¬пјЊжІЎжњ‰ж‰№е¤„зђ†ж€– powershellпјЊе¦‚жћњ Windows жІЎжњ‰её¦ bash е»єи®®и‡ЄиЎЊе®‰иЈ…гЂ‚

CMake еЏ‚ж•°з­‰з»†иЉ‚дёЋ Linux е¤§еђЊе°Џеј‚пјЊжњ‰й—®йўеЏЇд»ҐеЏ‚з…§ Build_Linux ж–‡жЎЈгЂ‚

#### зј–иЇ‘жњ¬дЅ“

иЇ·ж №жЌ®дЅ зљ„ QT Sdk зљ„дЅЌзЅ®ж›їжЌўе‘Ѕд»¤

```shell
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/path/to/qt/5.15.2/msvc2019_64 ..
ninja
```

зј–иЇ‘е®Њж€ђеђЋеѕ—е€° `cofebox.exe`

жњЂеђЋиїђиЎЊ `windeployqt cofebox.exe` и‡ЄеЉЁе¤Ќе€¶ж‰ЂйњЂ DLL з­‰ж–‡д»¶е€°еЅ“е‰Ќз›®еЅ•

### Go йѓЁе€†зј–иЇ‘

иЇ·зњ‹ [Build_Core.md](./Build_Core.md)



