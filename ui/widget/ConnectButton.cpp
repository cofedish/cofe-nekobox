#include "ConnectButton.h"

#include "ui/Typography.hpp"

#include <QPainter>
#include <QPainterPath>
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
    return Typography::ScalePx(210);
}

int ConnectButton::maximumDiameter() const {
    return Typography::ScalePx(300);
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

    QColor accent = palette().color(QPalette::Highlight);
    QColor textColor = palette().color(QPalette::HighlightedText);
    QColor coreDark = QColor(26, 11, 45);
    QColor coreLight = accent.lighter(135);
    QColor ringColor = accent.lighter(110);
    QColor glowColor = accent;

    if (current_state == State::Disconnected) {
        coreLight = accent.darker(170);
        textColor = palette().color(QPalette::WindowText);
        ringColor = accent;
        glowColor = accent.darker(130);
    }
    if (isDown()) {
        coreLight = coreLight.darker(112);
    } else if (underMouse()) {
        coreLight = coreLight.lighter(108);
    }

    qreal pulse = 1.0;
    if (!reduce_motion) {
        pulse = 1.0 + 0.028 * qSin(phase * 1.1);
    }

    const qreal outerRadius = (size * 0.5 - Typography::ScalePx(8)) * pulse;
    const qreal innerRadius = outerRadius - Typography::ScalePx(12);
    const QRectF outerRect(center.x() - outerRadius, center.y() - outerRadius, outerRadius * 2.0, outerRadius * 2.0);
    const QRectF innerRect(center.x() - innerRadius, center.y() - innerRadius, innerRadius * 2.0, innerRadius * 2.0);

    // Glow halo
    QRadialGradient glowGrad(center, outerRadius + Typography::ScalePx(24));
    QColor glowOuter = glowColor;
    glowOuter.setAlpha(0);
    QColor glowInner = glowColor;
    glowInner.setAlpha(current_state == State::Disconnected ? 72 : 136);
    glowGrad.setColorAt(0.0, glowInner);
    glowGrad.setColorAt(0.75, QColor(glowInner.red(), glowInner.green(), glowInner.blue(), 44));
    glowGrad.setColorAt(1.0, glowOuter);
    painter.setPen(Qt::NoPen);
    painter.setBrush(glowGrad);
    painter.drawEllipse(outerRect.adjusted(-Typography::ScalePx(14),
                                           -Typography::ScalePx(14),
                                           Typography::ScalePx(14),
                                           Typography::ScalePx(14)));

    // Inner sphere
    const QPointF gradientCenter(innerRect.center().x() - innerRect.width() * 0.18, innerRect.center().y() - innerRect.height() * 0.22);
    QRadialGradient coreGrad(gradientCenter, innerRadius * 1.2, innerRect.center());
    coreGrad.setColorAt(0.0, coreLight);
    coreGrad.setColorAt(0.72, coreDark);
    coreGrad.setColorAt(1.0, QColor(9, 4, 19));
    painter.setBrush(coreGrad);
    painter.drawEllipse(innerRect);

    // Neon ring
    QConicalGradient ringGrad(center, -qRadiansToDegrees(phase) * 1.2);
    QColor ringA = ringColor;
    QColor ringB = ringColor.lighter(160);
    ringA.setAlpha(220);
    ringB.setAlpha(250);
    ringGrad.setColorAt(0.0, ringA);
    ringGrad.setColorAt(0.35, ringB);
    ringGrad.setColorAt(0.7, ringA.darker(110));
    ringGrad.setColorAt(1.0, ringA);

    QPen ringPen(QBrush(ringGrad), Typography::ScalePx(6));
    ringPen.setCapStyle(Qt::RoundCap);
    painter.setBrush(Qt::NoBrush);
    if (current_state == State::Connecting || current_state == State::Disconnecting) {
        const int startDeg = static_cast<int>(qRadiansToDegrees(phase * 1.8) * 16.0);
        painter.setPen(ringPen);
        painter.drawArc(outerRect.adjusted(Typography::ScalePx(5),
                                           Typography::ScalePx(5),
                                           -Typography::ScalePx(5),
                                           -Typography::ScalePx(5)),
                        startDeg,
                        2880);
    } else {
        painter.setPen(ringPen);
        painter.drawEllipse(outerRect.adjusted(Typography::ScalePx(5),
                                               Typography::ScalePx(5),
                                               -Typography::ScalePx(5),
                                               -Typography::ScalePx(5)));
    }

    const auto text = stateText();
    QFont font = resolveTextFont(text, innerRect.width());
    painter.setFont(font);
    QPainterPath textPath;
    textPath.addText(0, 0, font, text);
    const QRectF textRect = textPath.boundingRect();
    QTransform tf;
    tf.translate(innerRect.center().x() - textRect.width() * 0.5, innerRect.center().y() + textRect.height() * 0.32);
    const auto centeredPath = tf.map(textPath);

    QColor textGlow = ringColor;
    textGlow.setAlpha(96);
    painter.setPen(QPen(textGlow, Typography::ScalePx(3), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(centeredPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(textColor);
    painter.drawPath(centeredPath);
}
