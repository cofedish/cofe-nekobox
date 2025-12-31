#include "WaveBackground.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

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

    QLinearGradient bg(bounds.topLeft(), bounds.bottomRight());
    bg.setColorAt(0.0, base.lighter(105));
    bg.setColorAt(1.0, surface.darker(110));
    painter.fillRect(bounds, bg);

    const qreal width = bounds.width();
    const qreal height = bounds.height();
    if (width <= 1.0 || height <= 1.0) {
        return;
    }

    const qreal base_y = height * 0.58;
    const qreal amplitude = qMin(height * 0.08, 60.0);
    const int steps = 48;
    const qreal step_x = width / steps;
    const qreal phase_a = phase;
    const qreal phase_b = -phase * 1.4;

    auto waveColorA = accent;
    waveColorA.setAlpha(46);
    auto waveColorB = accent;
    waveColorB.setAlpha(28);

    QPainterPath waveA;
    waveA.moveTo(0, base_y);
    for (int i = 0; i <= steps; ++i) {
        const qreal x = i * step_x;
        const qreal t = (x / width) * M_PI * 2.0;
        const qreal y = base_y + amplitude * qSin(t + phase_a);
        waveA.lineTo(x, y);
    }
    waveA.lineTo(width, height);
    waveA.lineTo(0, height);
    waveA.closeSubpath();

    QLinearGradient waveGradA(0, base_y - amplitude, 0, height);
    waveGradA.setColorAt(0.0, waveColorA);
    waveGradA.setColorAt(1.0, QColor(waveColorA.red(), waveColorA.green(), waveColorA.blue(), 0));
    painter.fillPath(waveA, waveGradA);

    const qreal base_y_b = base_y + amplitude * 0.35;
    QPainterPath waveB;
    waveB.moveTo(0, base_y_b);
    for (int i = 0; i <= steps; ++i) {
        const qreal x = i * step_x;
        const qreal t = (x / width) * M_PI * 2.0;
        const qreal y = base_y_b + amplitude * 0.65 * qSin(t + phase_b);
        waveB.lineTo(x, y);
    }
    waveB.lineTo(width, height);
    waveB.lineTo(0, height);
    waveB.closeSubpath();

    QLinearGradient waveGradB(0, base_y_b - amplitude, 0, height);
    waveGradB.setColorAt(0.0, waveColorB);
    waveGradB.setColorAt(1.0, QColor(waveColorB.red(), waveColorB.green(), waveColorB.blue(), 0));
    painter.fillPath(waveB, waveGradB);
}
