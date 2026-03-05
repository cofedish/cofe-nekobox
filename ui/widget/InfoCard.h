#pragma once

#include <QToolButton>

class QGraphicsDropShadowEffect;
class QVariantAnimation;

class InfoCard : public QToolButton {
public:
    explicit InfoCard(QWidget *parent = nullptr);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    void animateTo(qreal target);
    void setHoverProgress(qreal progress);
    void syncShadowColor();

    qreal hover_progress_ = 0.0;
    QGraphicsDropShadowEffect *shadow_ = nullptr;
    QVariantAnimation *hover_anim_ = nullptr;
};

