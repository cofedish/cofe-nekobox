# Build and Run

## Windows (dev)
1) Build C++ deps: `bash ./libs/build_deps_all.sh`
2) Configure: `cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/path/to/Qt/...`
3) Build: `cmake --build build`
4) Run: `build/cofebox.exe`

## Windows (release)
1) Build C++ deps: `bash ./libs/build_deps_all.sh`
2) Configure: `cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/path/to/Qt/...`
3) Build: `cmake --build build`
4) Deploy Qt runtime: `windeployqt build/cofebox.exe`

## Linux (dev)
1) Install Qt dev packages (qtbase/qtsvg/qttools/qtx11extras) and deps (protobuf/yaml-cpp/zxing-cpp).
2) Configure: `cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug`
3) Build: `cmake --build build`
4) Run: `build/cofebox`

## Linux (release)
1) Configure: `cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release`
2) Build: `cmake --build build`
3) Run: `build/cofebox`

## Go core
- Fetch sources: `bash libs/get_source.sh`
- Build: `GOOS=windows GOARCH=amd64 bash libs/build_go.sh`

See `docs/Build_Windows.md`, `docs/Build_Linux.md`, and `docs/Build_Core.md` for full details.

