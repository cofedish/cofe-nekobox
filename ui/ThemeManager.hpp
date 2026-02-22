#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QVector>

class QEvent;

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
    ThemeManager() = default;
    QString system_style_name = "";
    QString current_theme = "0"; // int: 0:system 1+:builtin string: QStyleFactory

    void ApplyTheme(const QString &theme);
    QVector<ThemeOption> AvailableThemes() const;
    ThemeOption ThemeOptionFor(const QString &theme) const;

signals:
    void themeChanged(const QString &theme);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool applying_theme_ = false;
    bool event_filter_installed_ = false;
};

extern ThemeManager *themeManager;
