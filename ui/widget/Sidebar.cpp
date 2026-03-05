#include "Sidebar.h"

Sidebar::Sidebar(QWidget *parent)
    : QFrame(parent) {
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);
}

