#include "MobileSafeArea.h"

MobileSafeArea::MobileSafeArea(QObject* parent) : QObject(parent) {
}

int MobileSafeArea::left() const {
    return left_;
}

int MobileSafeArea::top() const {
    return top_;
}

int MobileSafeArea::right() const {
    return right_;
}

int MobileSafeArea::bottom() const {
    return bottom_;
}

void MobileSafeArea::refresh() {
}
