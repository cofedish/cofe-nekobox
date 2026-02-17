#pragma once

#include <QFont>
#include <QString>

class QApplication;

namespace Typography {

enum class Role {
    AppBase,
    HomeTitle,
    HomeStatus,
    ConnectButtonState
};

int ScalePx(int px);
QString QssFamilyChain();
QFont FontForRole(Role role, const QFont &fallback = QFont{});
QString FontDebugString(const QFont &font);
void ApplyAppFont(QApplication &app);

} // namespace Typography
