#include "ToastWidget.h"

#include <QHBoxLayout>
#include <QStyle>
#include <QVariant>

ToastWidget::ToastWidget(QWidget *parent)
    : QFrame(parent) {
    setObjectName("toast_widget");
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setVisible(false);

    label = new QLabel(this);
    label->setWordWrap(true);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->addWidget(label);
    setLayout(layout);

    hide_timer = new QTimer(this);
    hide_timer->setSingleShot(true);
    connect(hide_timer, &QTimer::timeout, this, [=] { setVisible(false); });
}

void ToastWidget::showMessage(const QString &message, Level level, int durationMs) {
    label->setText(message);
    updateStyle(level);
    updatePosition();
    raise();
    setVisible(true);
    hide_timer->start(durationMs);
}

void ToastWidget::setAnchorRect(const QRect &rect) {
    anchor_rect = rect;
    if (isVisible()) {
        updatePosition();
    }
}

void ToastWidget::updateStyle(Level level) {
    QString levelName = "info";
    if (level == Level::Success) levelName = "success";
    if (level == Level::Warning) levelName = "warning";
    if (level == Level::Error) levelName = "error";
    setProperty("level", QVariant(levelName));
    style()->unpolish(this);
    style()->polish(this);
}

void ToastWidget::updatePosition() {
    auto rect = anchor_rect.isNull() && parentWidget() ? parentWidget()->rect() : anchor_rect;
    if (rect.isNull()) return;

    const int horizontalMargin = 16;
    const int verticalMargin = 24;
    const int maxWidth = qMax(160, rect.width() - horizontalMargin * 2);
    label->setMaximumWidth(maxWidth - 24);
    adjustSize();

    const int x = rect.x() + (rect.width() - width()) / 2;
    const int y = rect.y() + rect.height() - height() - verticalMargin;
    move(x, y);
}
