#include "GamepadBackend.h"
#import <GameController/GameController.h>

namespace {
constexpr quint32 LEFT=1, RIGHT=2, UP=4, DOWN=8, A=16, B=32, SELECT=64;

class MacGamepadBackend final : public GamepadBackend {
public:
    explicit MacGamepadBackend(QObject *parent=nullptr):GamepadBackend(parent) {
        center_ = [NSNotificationCenter defaultCenter];
        connectObs_ = [center_ addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){ attachFirst(); }];
        disconnectObs_ = [center_ addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){ attachFirst(); }];
        attachFirst();
    }
    ~MacGamepadBackend() override {
        if (controller_.extendedGamepad) controller_.extendedGamepad.valueChangedHandler = nil;
        if (connectObs_) [center_ removeObserver:connectObs_];
        if (disconnectObs_) [center_ removeObserver:disconnectObs_];
    }
private:
    void update(GCExtendedGamepad *p) {
        if (!p) { publish(0); return; }
        quint32 m = 0;
        const float x = p.leftThumbstick.xAxis.value;
        const float y = p.leftThumbstick.yAxis.value;
        if (p.dpad.left.isPressed || x < -0.45f) m |= LEFT;
        if (p.dpad.right.isPressed || x > 0.45f) m |= RIGHT;
        if (p.dpad.up.isPressed || y > 0.45f) m |= UP;
        if (p.dpad.down.isPressed || y < -0.45f) m |= DOWN;
        if (p.buttonA.isPressed) m |= A;
        if (p.buttonB.isPressed) m |= B;
        if (@available(macOS 10.15, iOS 13.0, *)) {
            if (p.buttonMenu.isPressed || p.buttonOptions.isPressed) m |= SELECT;
        }
        publish(m);
    }
    void attachFirst() {
        if (controller_.extendedGamepad) controller_.extendedGamepad.valueChangedHandler = nil;
        NSArray<GCController*> *controllers = [GCController controllers];
        controller_ = controllers.count ? controllers.firstObject : nil;
        if (!controller_) { setState(false, {}); publish(0); return; }
        NSString *vendor = controller_.vendorName ?: @"Game Controller";
        setState(true, QString::fromUtf8([vendor UTF8String]));
        GCExtendedGamepad *pad = controller_.extendedGamepad;
        if (!pad) { publish(0); return; }
        pad.valueChangedHandler = ^(GCExtendedGamepad *gamepad, GCControllerElement *) { update(gamepad); };
        update(pad);
    }
    NSNotificationCenter *center_ = nil;
    id connectObs_ = nil;
    id disconnectObs_ = nil;
    GCController *controller_ = nil;
};
}
GamepadBackend *createGamepadBackend(QObject *parent) { return new MacGamepadBackend(parent); }
