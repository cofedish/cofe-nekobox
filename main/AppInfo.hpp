#pragma once

#include <QString>

#ifndef APP_DISPLAY_NAME
#define APP_DISPLAY_NAME "CofeBox"
#endif

#ifndef APP_ID
#define APP_ID "cofebox"
#endif

#ifndef APP_CONFIG_ID
#define APP_CONFIG_ID "nekoray"
#endif

#ifndef APP_VERSION_STR
#define APP_VERSION_STR "0.0.0"
#endif

namespace AppInfo {

inline QString DisplayName() {
    return QStringLiteral(APP_DISPLAY_NAME);
}

inline QString AppId() {
    return QStringLiteral(APP_ID);
}

inline QString ConfigAppId() {
    return QStringLiteral(APP_CONFIG_ID);
}

inline QString VersionRaw() {
    return QStringLiteral(APP_VERSION_STR);
}

inline QString Version() {
    auto v = VersionRaw().trimmed();
    if (v.startsWith("v", Qt::CaseInsensitive)) {
        v.remove(0, 1);
    }
    return v.isEmpty() ? QStringLiteral("0.0.0") : v;
}

inline QString RepoUrl() {
    return QStringLiteral("https://github.com/cofedish/cofe-nekobox");
}

inline QString DocsUrl() {
    return QStringLiteral("https://github.com/cofedish/cofe-nekobox/tree/main/docs");
}

} // namespace AppInfo
