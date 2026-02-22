#!/bin/bash
set -e

source libs/env_deploy.sh
DEST=$DEPLOYMENT/windows64
rm -rf $DEST
mkdir -p $DEST

#### copy exe ####
cp $BUILD/cofebox.exe $DEST
[ -f "$BUILD/cofebox-net-helper.exe" ] && cp "$BUILD/cofebox-net-helper.exe" "$DEST"

#### deploy qt & DLL runtime ####
pushd $DEST
windeployqt cofebox.exe --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw --verbose 2
rm -rf translations
rm -rf libEGL.dll libGLESv2.dll Qt6Pdf.dll

if [ "$DL_QT_VER" != "5.15" ]; then
  [ -f "$SRC_ROOT/qtsdk/Qt/bin/libcrypto-3-x64.dll" ] && cp "$SRC_ROOT/qtsdk/Qt/bin/libcrypto-3-x64.dll" .
  [ -f "$SRC_ROOT/qtsdk/Qt/bin/libssl-3-x64.dll" ] && cp "$SRC_ROOT/qtsdk/Qt/bin/libssl-3-x64.dll" .
fi

popd

#### prepare deployment ####
if compgen -G "$BUILD/*.pdb" > /dev/null; then
  cp "$BUILD"/*.pdb "$DEPLOYMENT"
fi
