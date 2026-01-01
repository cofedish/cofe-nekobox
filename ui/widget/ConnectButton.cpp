#include "ConnectButton.h"

#include <QPainter>
#include <QTimer>
#include <QtMath>

ConnectButton::ConnectButton(QWidget *parent)
    : QPushButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setCheckable(false);
    setFlat(true);

    timer = new QTimer(this);
    timer->setInterval(33);
    connect(timer, &QTimer::timeout, this, [=] {
        phase += 0.08;
        if (phase > M_PI * 2) phase = 0.0;
        update();
    });
    updateAnimationState();
}

void ConnectButton::setState(State state) {
    if (current_state == state) return;
    current_state = state;
    updateAnimationState();
    update();
}

ConnectButton::State ConnectButton::state() const {
    return current_state;
}

void ConnectButton::setReduceMotion(bool reduce) {
    reduce_motion = reduce;
    updateAnimationState();
    update();
}

bool ConnectButton::reduceMotion() const {
    return reduce_motion;
}

QString ConnectButton::stateText() const {
    switch (current_state) {
        case State::Disconnected:
            return tr("Disconnected");
        case State::Connecting:
            return tr("Connecting");
        case State::Connected:
            return tr("Connected");
        case State::Disconnecting:
            return tr("Disconnecting");
    }
    return tr("Connect");
}

void ConnectButton::updateAnimationState() {
    if (reduce_motion) {
        if (timer->isActive()) timer->stop();
        return;
    }
    if (!timer->isActive()) timer->start();
}

void ConnectButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect();
    const qreal size = qMin(bounds.width(), bounds.height());
    const QPointF center = bounds.center();

    qreal pulse = 1.0;
    if (!reduce_motion && (current_state == State::Disconnected || current_state == State::Connected)) {
        pulse = 1.0 + 0.02 * qSin(phase);
    }

    const qreal radius = (size * 0.5 - 10.0) * pulse;
    QRectF circleRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);

    QColor accent = palette().color(QPalette::Highlight);
    QColor base = palette().color(QPalette::Button);
    QColor textColor = palette().color(QPalette::ButtonText);
    QColor ringColor = accent;
    QColor ringSoft = accent;
    ringSoft.setAlpha(60);

    if (current_state == State::Connected || current_state == State::Connecting || current_state == State::Disconnecting) {
        base = accent;
        textColor = palette().color(QPalette::HighlightedText);
    }

    if (isDown()) {
        base = base.darker(110);
    } else if (underMouse()) {
        base = base.lighter(105);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawEllipse(circleRect);

    QPen ringPen(ringColor, 6.0);
    ringPen.setCapStyle(Qt::RoundCap);
    painter.setBrush(Qt::NoBrush);

    if (current_state == State::Connecting || current_state == State::Disconnecting) {
        qreal startDeg = qRadiansToDegrees(phase) * 16.0;
        qreal spanDeg = 270.0 * 16.0;
        painter.setPen(ringPen);
        painter.drawArc(circleRect.adjusted(6, 6, -6, -6), static_cast<int>(startDeg), static_cast<int>(spanDeg));
    } else if (current_state == State::Connected) {
        ringPen.setColor(ringSoft);
        painter.setPen(ringPen);
        painter.drawEllipse(circleRect.adjusted(6, 6, -6, -6));
    } else {
        ringPen.setColor(ringSoft);
        painter.setPen(ringPen);
        painter.drawEllipse(circleRect.adjusted(8, 8, -8, -8));
    }

    QFont font = this->font();
    font.setBold(true);
    font.setPointSizeF(qMax(9.0, size * 0.09));
    painter.setFont(font);
    painter.setPen(textColor);
    painter.drawText(circleRect, Qt::AlignCenter, stateText());
}
