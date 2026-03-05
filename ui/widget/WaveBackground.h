#pragma once

#include <QWidget>
#include <QPointF>

class QTimer;

class WaveBackground : public QWidget {
    Q_OBJECT

public:
    explicit WaveBackground(QWidget *parent = nullptr);

    void setReduceMotion(bool reduce);
    [[nodiscard]] bool reduceMotion() const;

    // Normalized [-1..1] parallax offset driven by cursor position
    void setParallaxOffset(QPointF offset);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer *timer = nullptr;
    qreal phase = 0.0;
    bool reduce_motion = false;
    QPointF parallax_offset;          // current target
    QPointF parallax_current;         // smoothed value
};
