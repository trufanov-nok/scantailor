#include "ClickableLabel.h"

namespace publish {

ClickableLabel::ClickableLabel(QWidget* parent)
    : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
}

ClickableLabel::~ClickableLabel() {}

void ClickableLabel::mousePressEvent(QMouseEvent* /*event*/) {
    emit clicked();
}

}
