#include "RightInfoPanel.h"

RightInfoPanel::RightInfoPanel(QWidget *parent)
    : QFrame(parent) {
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);
}

