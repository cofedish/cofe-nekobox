#pragma once

#include <QFrame>
#include <QLabel>
#include <QTimer>

class ToastWidget : public QFrame {
    Q_OBJECT

public:
    enum class Level {
        Info,
        Success,
        Warning,
        Error
    };

    explicit ToastWidget(QWidget *parent = nullptr);

    void showMessage(const QString &message, Level level = Level::Info, int durationMs = 2500);

    void setAnchorRect(const QRect &rect);

private:
    void updateStyle(Level level);
    void updatePosition();

    QLabel *label = nullptr;
    QTimer *hide_timer = nullptr;
    QRect anchor_rect;
};
