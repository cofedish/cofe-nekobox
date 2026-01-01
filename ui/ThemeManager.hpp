#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QVector>

struct ThemeOption {
    QString id;
    QString displayName;
    QColor window;
    QColor surface;
    QColor accent;
    QColor text;
};

class ThemeManager : public QObject {
    Q_OBJECT

public:
    QString system_style_name = "";
    QString current_theme = "0"; // int: 0:system 1+:builtin string: QStyleFactory

    void ApplyTheme(const QString &theme);
    QVector<ThemeOption> AvailableThemes() const;
    ThemeOption ThemeOptionFor(const QString &theme) const;

signals:
    void themeChanged(const QString &theme);
};

extern ThemeManager *themeManager;
