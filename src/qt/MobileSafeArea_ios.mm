#include "MobileSafeArea.h"

#import <UIKit/UIKit.h>

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
    UIWindow* activeWindow = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (scene.activationState != UISceneActivationStateForegroundActive ||
            ![scene isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        for (UIWindow* candidate in ((UIWindowScene*)scene).windows) {
            if (candidate.isKeyWindow) {
                activeWindow = candidate;
                break;
            }
        }
        if (activeWindow != nil)
            break;
    }
    if (activeWindow == nil)
        return;
    UIEdgeInsets insets = activeWindow.safeAreaInsets;
    int nextLeft = int(insets.left);
    int nextTop = int(insets.top);
    int nextRight = int(insets.right);
    int nextBottom = int(insets.bottom);
    if (left_ == nextLeft && top_ == nextTop && right_ == nextRight && bottom_ == nextBottom)
        return;
    left_ = nextLeft;
    top_ = nextTop;
    right_ = nextRight;
    bottom_ = nextBottom;
    emit marginsChanged();
}
