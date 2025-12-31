#pragma once

#include <QWidget>

class QTimer;

class WaveBackground : public QWidget {
    Q_OBJECT

public:
    explicit WaveBackground(QWidget *parent = nullptr);

    void setReduceMotion(bool reduce);
    [[nodiscard]] bool reduceMotion() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer *timer = nullptr;
    qreal phase = 0.0;
    bool reduce_motion = false;
};
