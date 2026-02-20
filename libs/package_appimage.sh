#!/bin/bash

sudo apt-get install fuse -y

cp -r linux64 CofeBox.AppDir

# The file for Appimage

rm CofeBox.AppDir/launcher

cat >CofeBox.AppDir/CofeBox.desktop <<-EOF
[Desktop Entry]
Name=CofeBox
Exec=cofebox -appdata
Icon=cofebox
Type=Application
Categories=Network
EOF

cat >CofeBox.AppDir/AppRun <<-EOF
#!/bin/bash
echo "PATH: \${PATH}"
echo "cofebox running on: \$APPDIR"
LD_LIBRARY_PATH=\${APPDIR}/usr/lib QT_PLUGIN_PATH=\${APPDIR}/usr/plugins \${APPDIR}/cofebox -appdata "\$@"
EOF

chmod +x CofeBox.AppDir/AppRun

# build

appimage_urls=(
    "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    "https://github.com/AppImage/AppImageKit/releases/latest/download/appimagetool-x86_64.AppImage"
)

for url in "${appimage_urls[@]}"; do
    if curl -fLSO "$url"; then
        break
    fi
done

if [ ! -f appimagetool-x86_64.AppImage ]; then
    echo "Failed to download appimagetool-x86_64.AppImage"
    exit 1
fi
chmod +x appimagetool-x86_64.AppImage
./appimagetool-x86_64.AppImage CofeBox.AppDir

# clean

rm appimagetool-x86_64.AppImage
rm -rf CofeBox.AppDir
