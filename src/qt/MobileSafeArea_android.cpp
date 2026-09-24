#include "MobileSafeArea.h"

#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#include <cmath>

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
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
    QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
    QJniObject windowInsets = decor.callObjectMethod("getRootWindowInsets", "()Landroid/view/WindowInsets;");
    if (!windowInsets.isValid())
        return;
    jint systemBars =
        QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type", "systemBars", "()I");
    jint displayCutout =
        QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type", "displayCutout", "()I");
    QJniObject insets = windowInsets.callObjectMethod(
        "getInsets", "(I)Landroid/graphics/Insets;", systemBars | displayCutout);
    QJniObject resources = activity.callObjectMethod("getResources", "()Landroid/content/res/Resources;");
    QJniObject metrics = resources.callObjectMethod("getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
    float density = metrics.getField<jfloat>("density");
    if (!insets.isValid() || density <= 0)
        return;
    int nextLeft = int(std::lround(insets.getField<jint>("left") / density));
    int nextTop = int(std::lround(insets.getField<jint>("top") / density));
    int nextRight = int(std::lround(insets.getField<jint>("right") / density));
    int nextBottom = int(std::lround(insets.getField<jint>("bottom") / density));
    if (left_ == nextLeft && top_ == nextTop && right_ == nextRight && bottom_ == nextBottom)
        return;
    left_ = nextLeft;
    top_ = nextTop;
    right_ = nextRight;
    bottom_ = nextBottom;
    emit marginsChanged();
}
