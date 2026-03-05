#include "WaveBackground.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTimer>
#include <QtMath>
#include <cmath>

WaveBackground::WaveBackground(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);

    timer = new QTimer(this);
    timer->setInterval(33);
    connect(timer, &QTimer::timeout, this, [=] {
        if (!reduce_motion) {
            phase += 0.025;
            if (phase > M_PI * 2) phase = 0.0;
            update();
        }
    });
    timer->start();
}

void WaveBackground::setReduceMotion(bool reduce) {
    reduce_motion = reduce;
    if (reduce_motion) {
        timer->stop();
    } else if (!timer->isActive()) {
        timer->start();
    }
    update();
}

bool WaveBackground::reduceMotion() const {
    return reduce_motion;
}

void WaveBackground::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect();
    const QColor base = palette().color(QPalette::Window);
    const QColor surface = palette().color(QPalette::Base);
    const QColor accent = palette().color(QPalette::Highlight);
    QColor violet = accent;
    violet = violet.lighter(120);
    violet.setHsv((violet.hsvHue() + 24 + 360) % 360, qBound(90, violet.hsvSaturation() + 20, 255), qBound(35, violet.value(), 255));

    QLinearGradient bg(bounds.topLeft(), bounds.bottomRight());
    bg.setColorAt(0.0, base.darker(118));
    bg.setColorAt(0.5, surface.darker(170));
    bg.setColorAt(1.0, base.darker(140));
    painter.fillRect(bounds, bg);

    const qreal width = bounds.width();
    const qreal height = bounds.height();
    if (width <= 1.0 || height <= 1.0) {
        return;
    }

    // Deep ambient blooms.
    QRadialGradient bloom1(width * 0.22, height * 0.18, qMax(width, height) * 0.62);
    QColor bloom1Color = accent;
    bloom1Color.setAlpha(62);
    bloom1.setColorAt(0.0, bloom1Color);
    bloom1.setColorAt(1.0, QColor(bloom1Color.red(), bloom1Color.green(), bloom1Color.blue(), 0));
    painter.fillRect(bounds, bloom1);

    QRadialGradient bloom2(width * 0.78, height * 0.8, qMax(width, height) * 0.55);
    QColor bloom2Color = violet;
    bloom2Color.setAlpha(54);
    bloom2.setColorAt(0.0, bloom2Color);
    bloom2.setColorAt(1.0, QColor(bloom2Color.red(), bloom2Color.green(), bloom2Color.blue(), 0));
    painter.fillRect(bounds, bloom2);

    // Star dust.
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 130; ++i) {
        const qreal noise = qSin(i * 12.9898 + phase * 7.3) * 43758.5453;
        const qreal fract = noise - std::floor(noise);
        const qreal x = (i * 91 % 997) / 997.0 * width;
        const qreal y = fract * height;
        const qreal twinkle = reduce_motion ? 0.75 : 0.5 + 0.5 * qSin(phase * 1.8 + i * 0.37);
        QColor dot = i % 3 == 0 ? violet : accent;
        dot.setAlpha(static_cast<int>(36 + 94 * twinkle));
        painter.setBrush(dot);
        const qreal r = (i % 7 == 0) ? 1.7 : 1.0;
        painter.drawEllipse(QPointF(x, y), r, r);
    }

    auto drawRibbon = [&](qreal yFactor, qreal amp, qreal widthPx, qreal speed, const QColor &c1, const QColor &c2, qreal opacity) {
        QPainterPath centerLine;
        const int steps = 110;
        const qreal baseY = height * yFactor;
        centerLine.moveTo(0, baseY);
        for (int i = 1; i <= steps; ++i) {
            const qreal x = width * (static_cast<qreal>(i) / steps);
            const qreal t = (x / width) * (M_PI * 3.0);
            const qreal y = baseY + amp * qSin(t + phase * speed);
            centerLine.lineTo(x, y);
        }

        QPainterPathStroker stroker;
        stroker.setWidth(widthPx);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath ribbon = stroker.createStroke(centerLine);

        QLinearGradient grad(0, baseY - amp, width, baseY + amp);
        QColor a = c1;
        QColor b = c2;
        a.setAlphaF(opacity);
        b.setAlphaF(opacity * 0.78);
        grad.setColorAt(0.0, a);
        grad.setColorAt(0.5, b);
        grad.setColorAt(1.0, a);
        painter.fillPath(ribbon, grad);
    };

    drawRibbon(0.42, qMin(height * 0.07, 58.0), qMin(height * 0.16, 74.0), 1.1, accent.lighter(140), violet.lighter(130), 0.33);
    drawRibbon(0.47, qMin(height * 0.075, 65.0), qMin(height * 0.12, 60.0), -0.9, accent, violet, 0.42);
    drawRibbon(0.53, qMin(height * 0.06, 48.0), qMin(height * 0.1, 52.0), 0.65, violet, accent.darker(115), 0.28);

    // Vignette.
    QRadialGradient vignette(width * 0.5, height * 0.5, qMax(width, height) * 0.72);
    vignette.setColorAt(0.6, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 130));
    painter.fillRect(bounds, vignette);
}
