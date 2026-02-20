#!/bin/bash

version="$1"
version="${version#v}"
version="${version#V}"

mkdir -p cofebox/DEBIAN
mkdir -p cofebox/opt
cp -r linux64 cofebox/opt/
mv cofebox/opt/linux64 cofebox/opt/cofebox
rm -rf cofebox/opt/cofebox/usr
rm cofebox/opt/cofebox/launcher

# basic
cat >cofebox/DEBIAN/control <<-EOF
Package: cofebox
Version: $version
Architecture: amd64
Maintainer: CofeDish cofedish@users.noreply.github.com
Depends: libxcb-xinerama0, libqt5core5a, libqt5gui5, libqt5network5, libqt5widgets5, libqt5svg5, libqt5x11extras5, desktop-file-utils
Description: Qt based cross-platform GUI proxy configuration manager (backend: v2ray / sing-box)
EOF

cat >cofebox/DEBIAN/postinst <<-EOF
if [ ! -s /usr/share/applications/cofebox.desktop ]; then
    cat >/usr/share/applications/cofebox.desktop<<-END
[Desktop Entry]
Name=CofeBox
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/cofebox:\$PATH /opt/cofebox/cofebox -appdata"
Icon=/opt/cofebox/cofebox.png
Terminal=false
Type=Application
Categories=Network;Application;
END
fi

setcap cap_net_admin=ep /opt/cofebox/cofebox_core

update-desktop-database
EOF

sudo chmod 0755 cofebox/DEBIAN/postinst

# desktop && PATH

sudo dpkg-deb -Zxz --build cofebox
