#include "TopBar.h"

TopBar::TopBar(QWidget *parent)
    : QFrame(parent) {
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);
}

