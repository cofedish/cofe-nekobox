#include "InfoCard.h"

#include <QEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif
#include <QGraphicsDropShadowEffect>
#include <QVariantAnimation>
#include <QEasingCurve>

InfoCard::InfoCard(QWidget *parent)
    : QToolButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setAutoRaise(false);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setAttribute(Qt::WA_StyledBackground, true);

    shadow_ = new QGraphicsDropShadowEffect(this);
    shadow_->setOffset(0.0, 1.0);
    shadow_->setBlurRadius(10.0);
    setGraphicsEffect(shadow_);

    hover_anim_ = new QVariantAnimation(this);
    hover_anim_->setDuration(190);
    hover_anim_->setEasingCurve(QEasingCurve::InOutCubic);
    connect(hover_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setHoverProgress(value.toReal());
    });

    syncShadowColor();
    setHoverProgress(0.0);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void InfoCard::enterEvent(QEnterEvent *event) {
#else
void InfoCard::enterEvent(QEvent *event) {
#endif
    animateTo(1.0);
    QToolButton::enterEvent(event);
}

void InfoCard::leaveEvent(QEvent *event) {
    animateTo(0.0);
    QToolButton::leaveEvent(event);
}

bool InfoCard::event(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        syncShadowColor();
        setHoverProgress(hover_progress_);
    }
    return QToolButton::event(event);
}

void InfoCard::animateTo(qreal target) {
    if (hover_anim_ == nullptr) return;
    hover_anim_->stop();
    hover_anim_->setStartValue(hover_progress_);
    hover_anim_->setEndValue(qBound(0.0, target, 1.0));
    hover_anim_->start();
}

void InfoCard::setHoverProgress(qreal progress) {
    hover_progress_ = qBound(0.0, progress, 1.0);
    if (shadow_ == nullptr) return;

    const qreal lift = 1.0 + hover_progress_ * 1.6;
    shadow_->setOffset(0.0, lift);
    shadow_->setBlurRadius(10.0 + hover_progress_ * 14.0);

    QColor glow = palette().color(QPalette::Highlight);
    glow.setAlpha(24 + static_cast<int>(hover_progress_ * 92.0));
    shadow_->setColor(glow);
}

void InfoCard::syncShadowColor() {
    if (shadow_ == nullptr) return;
    QColor glow = palette().color(QPalette::Highlight);
    glow.setAlpha(24 + static_cast<int>(hover_progress_ * 92.0));
    shadow_->setColor(glow);
}
