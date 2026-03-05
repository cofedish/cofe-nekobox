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
    setMouseTracking(true);

    timer = new QTimer(this);
    timer->setInterval(16); // ~60 fps
    connect(timer, &QTimer::timeout, this, [=] {
        if (!reduce_motion) {
            phase += 0.013;
            if (phase > M_PI * 2) phase = 0.0;
        }
        // Smooth parallax interpolation every frame (even in reduce_motion)
        const qreal lerp = 0.072;
        parallax_current.setX(parallax_current.x() + (parallax_offset.x() - parallax_current.x()) * lerp);
        parallax_current.setY(parallax_current.y() + (parallax_offset.y() - parallax_current.y()) * lerp);
        update();
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

void WaveBackground::setParallaxOffset(QPointF offset) {
    parallax_offset = offset;
}

void WaveBackground::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect();
    const QColor base    = palette().color(QPalette::Window);
    const QColor surface = palette().color(QPalette::Base);
    const QColor accent  = palette().color(QPalette::Highlight);

    // Secondary color: slightly shifted hue for variation
    QColor violet = accent;
    violet.setHsv((violet.hsvHue() + 26 + 360) % 360,
                  qBound(80, violet.hsvSaturation() + 15, 255),
                  qBound(30, violet.value(), 255));

    const qreal W = bounds.width();
    const qreal H = bounds.height();

    // --- Background gradient ---
    QLinearGradient bg(bounds.topLeft(), bounds.bottomRight());
    bg.setColorAt(0.0, base.darker(122));
    bg.setColorAt(0.45, surface.darker(180));
    bg.setColorAt(1.0, base.darker(145));
    painter.fillRect(bounds, bg);

    if (W <= 1.0 || H <= 1.0) return;

    // --- Ambient blooms (parallax-shifted) ---
    const qreal px = parallax_current.x();
    const qreal py = parallax_current.y();

    auto fillBloom = [&](qreal cx, qreal cy, qreal r, const QColor &c, int alpha) {
        QRadialGradient bloom(W * cx + px * W * 0.06, H * cy + py * H * 0.06, qMax(W, H) * r);
        QColor bc = c; bc.setAlpha(alpha);
        bloom.setColorAt(0.0, bc);
        bloom.setColorAt(1.0, QColor(bc.red(), bc.green(), bc.blue(), 0));
        painter.fillRect(bounds, bloom);
    };

    fillBloom(0.20, 0.15, 0.65, accent,  68);
    fillBloom(0.80, 0.82, 0.58, violet,  58);
    fillBloom(0.55, 0.42, 0.38, accent.lighter(115), 28);

    // --- Star dust particles ---
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 150; ++i) {
        const qreal noise  = qSin(i * 12.9898 + phase * 7.3) * 43758.5453;
        const qreal fract  = noise - std::floor(noise);
        const qreal xBase  = (i * 91 % 997) / 997.0 * W;
        const qreal yBase  = fract * H;
        // Slight parallax on stars based on "depth" (i % 3)
        const qreal depthF = (i % 3) * 0.012;
        const qreal x = xBase + px * W * depthF;
        const qreal y = yBase + py * H * depthF;
        const qreal twinkle = reduce_motion ? 0.75 : 0.5 + 0.5 * qSin(phase * 1.9 + i * 0.39);
        QColor dot = (i % 3 == 0) ? violet : accent;
        dot.setAlpha(static_cast<int>(28 + 108 * twinkle));
        painter.setBrush(dot);
        const qreal r = (i % 7 == 0) ? 1.8 : (i % 5 == 0 ? 1.3 : 0.9);
        painter.drawEllipse(QPointF(x, y), r, r);
    }

    // --- Ribbons with 3D depth via parallax ---
    // Each ribbon layer gets a Y offset proportional to its depth factor.
    // Closer layers (higher depth factor) move more with the cursor.
    // The specular (bright top edge) gives a 3D silk appearance.

    struct RibbonDef {
        qreal yFactor;
        qreal amp;
        qreal widthPx;
        qreal speed;
        QColor c1, c2;
        qreal opacity;
        qreal depth;      // 0=far, 1=near; drives parallax scale
    };

    const qreal ampScale = H;
    const QList<RibbonDef> ribbons = {
        { 0.36, qMin(ampScale * 0.055, 48.0), qMin(ampScale * 0.12, 60.0),  1.4,  accent.lighter(165), violet.lighter(148), 0.18, 0.10 },
        { 0.41, qMin(ampScale * 0.075, 62.0), qMin(ampScale * 0.17, 82.0),  1.1,  accent.lighter(142), violet.lighter(132), 0.40, 0.28 },
        { 0.47, qMin(ampScale * 0.090, 74.0), qMin(ampScale * 0.15, 72.0), -0.9,  accent,               violet,              0.55, 0.50 },
        { 0.53, qMin(ampScale * 0.072, 58.0), qMin(ampScale * 0.12, 58.0),  0.65, violet,               accent.darker(112),  0.38, 0.72 },
        { 0.59, qMin(ampScale * 0.055, 44.0), qMin(ampScale * 0.09, 48.0), -0.50, accent.darker(118),   violet.darker(108),  0.22, 0.90 },
    };

    const int steps = 120;

    for (const auto &rd : ribbons) {
        const qreal yParallax = py * H * 0.045 * rd.depth;
        const qreal baseY = H * rd.yFactor + yParallax;

        // Build center path
        QPainterPath centerLine;
        centerLine.moveTo(0, baseY);
        for (int i = 1; i <= steps; ++i) {
            const qreal x = W * (static_cast<qreal>(i) / steps);
            const qreal t = (x / W) * (M_PI * 3.2);
            const qreal y = baseY + rd.amp * qSin(t + phase * rd.speed);
            centerLine.lineTo(x, y);
        }

        // Ribbon body
        QPainterPathStroker stroker;
        stroker.setWidth(rd.widthPx);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        const QPainterPath ribbon = stroker.createStroke(centerLine);

        QLinearGradient grad(0, baseY - rd.amp, W, baseY + rd.amp);
        QColor a = rd.c1; a.setAlphaF(rd.opacity);
        QColor b = rd.c2; b.setAlphaF(rd.opacity * 0.76);
        grad.setColorAt(0.0, a);
        grad.setColorAt(0.5, b);
        grad.setColorAt(1.0, a);
        painter.setPen(Qt::NoPen);
        painter.fillPath(ribbon, grad);

        // 3D specular highlight — thin bright stroke along the top edge
        QPainterPathStroker specStroker;
        specStroker.setWidth(qMax(1.5, rd.widthPx * 0.12));
        specStroker.setCapStyle(Qt::RoundCap);
        specStroker.setJoinStyle(Qt::RoundJoin);

        // Top-edge path: offset centerLine slightly upward
        QPainterPath topEdge;
        topEdge.moveTo(0, baseY - rd.widthPx * 0.28);
        for (int i = 1; i <= steps; ++i) {
            const qreal x = W * (static_cast<qreal>(i) / steps);
            const qreal t = (x / W) * (M_PI * 3.2);
            const qreal y = (baseY - rd.widthPx * 0.28) + rd.amp * qSin(t + phase * rd.speed);
            topEdge.lineTo(x, y);
        }
        const QPainterPath specPath = specStroker.createStroke(topEdge);

        QLinearGradient specGrad(0, baseY - rd.amp, W, baseY + rd.amp);
        QColor specC = rd.c1.lighter(185);
        specC.setAlphaF(rd.opacity * 0.55);
        QColor specZ = specC; specZ.setAlpha(0);
        specGrad.setColorAt(0.0, specC);
        specGrad.setColorAt(0.5, specZ);
        specGrad.setColorAt(1.0, specC);
        painter.fillPath(specPath, specGrad);
    }

    // --- Vignette ---
    QRadialGradient vignette(W * 0.5, H * 0.5, qMax(W, H) * 0.74);
    vignette.setColorAt(0.55, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0,  QColor(0, 0, 0, 140));
    painter.fillRect(bounds, vignette);
}
