#pragma once

#include <QtGlobal>
#include <QToolButton>

class QGraphicsDropShadowEffect;
class QVariantAnimation;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QEnterEvent;
#endif

class InfoCard : public QToolButton {
public:
    explicit InfoCard(QWidget *parent = nullptr);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
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
