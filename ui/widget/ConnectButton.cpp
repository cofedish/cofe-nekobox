#include "ConnectButton.h"

#include "ui/Typography.hpp"

#include <QPainter>
#include <QTimer>
#include <QSizePolicy>
#include <QtMath>

ConnectButton::ConnectButton(QWidget *parent)
    : QPushButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setCheckable(false);
    setFlat(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    refreshMetrics();

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
    refreshMetrics();
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

int ConnectButton::minimumDiameter() const {
    return Typography::ScalePx(180);
}

int ConnectButton::maximumDiameter() const {
    return Typography::ScalePx(260);
}

int ConnectButton::targetDiameter() const {
    const auto text = stateText();
    auto font = Typography::FontForRole(Typography::Role::ConnectButtonState, this->font());
    QFontMetrics metrics(font);
    const int horizontalPadding = Typography::ScalePx(56);
    const int verticalPadding = Typography::ScalePx(64);
    const int need = qMax(metrics.horizontalAdvance(text) + horizontalPadding,
                          metrics.height() + verticalPadding);
    return qBound(minimumDiameter(), need, maximumDiameter());
}

QFont ConnectButton::resolveTextFont(const QString &text, qreal diameter) const {
    QFont font = Typography::FontForRole(Typography::Role::ConnectButtonState, this->font());
    font.setWeight(QFont::Medium);
    const int minPx = Typography::ScalePx(14);
    const int maxPx = Typography::ScalePx(24);
    const int widthLimit = qMax(1, qRound(diameter) - Typography::ScalePx(46));
    const int heightLimit = qMax(1, qRound(diameter) - Typography::ScalePx(58));
    int px = qBound(minPx, font.pixelSize(), maxPx);
    for (; px >= minPx; --px) {
        font.setPixelSize(px);
        QFontMetrics fm(font);
        if (fm.horizontalAdvance(text) <= widthLimit && fm.height() <= heightLimit) {
            break;
        }
    }
    return font;
}

void ConnectButton::refreshMetrics() {
    const int minDia = minimumDiameter();
    const int maxDia = maximumDiameter();
    const int target = targetDiameter();
    setMinimumSize(minDia, minDia);
    setMaximumSize(maxDia, maxDia);
    resize(target, target);
    updateGeometry();
}

QSize ConnectButton::sizeHint() const {
    const int dia = targetDiameter();
    return {dia, dia};
}

QSize ConnectButton::minimumSizeHint() const {
    const int dia = minimumDiameter();
    return {dia, dia};
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
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF bounds = rect();
    const qreal size = qMin(bounds.width(), bounds.height());
    const QPointF center = bounds.center();

    qreal pulse = 1.0;
    if (!reduce_motion && (current_state == State::Disconnected || current_state == State::Connected)) {
        pulse = 1.0 + 0.02 * qSin(phase);
    }

    const qreal ringInset = Typography::ScalePx(10);
    const qreal radius = (size * 0.5 - ringInset) * pulse;
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

    QPen ringPen(ringColor, Typography::ScalePx(6));
    ringPen.setCapStyle(Qt::RoundCap);
    painter.setBrush(Qt::NoBrush);

    if (current_state == State::Connecting || current_state == State::Disconnecting) {
        qreal startDeg = qRadiansToDegrees(phase) * 16.0;
        qreal spanDeg = 270.0 * 16.0;
        painter.setPen(ringPen);
        const int arcInset = Typography::ScalePx(6);
        painter.drawArc(circleRect.adjusted(arcInset, arcInset, -arcInset, -arcInset), static_cast<int>(startDeg),
                        static_cast<int>(spanDeg));
    } else if (current_state == State::Connected) {
        ringPen.setColor(ringSoft);
        painter.setPen(ringPen);
        const int arcInset = Typography::ScalePx(6);
        painter.drawEllipse(circleRect.adjusted(arcInset, arcInset, -arcInset, -arcInset));
    } else {
        ringPen.setColor(ringSoft);
        painter.setPen(ringPen);
        const int arcInset = Typography::ScalePx(8);
        painter.drawEllipse(circleRect.adjusted(arcInset, arcInset, -arcInset, -arcInset));
    }

    const auto text = stateText();
    QFont font = resolveTextFont(text, circleRect.width());
    painter.setFont(font);
    painter.setPen(textColor);
    painter.drawText(circleRect, Qt::AlignCenter, text);
}
