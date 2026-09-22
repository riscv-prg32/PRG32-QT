#include "GamepadBackend.h"
#include "InputButtons.h"
#import <GameController/GameController.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <IOKit/hid/IOHIDManager.h>
#endif
#include <array>

namespace {
class MacGamepadBackend final : public GamepadBackend {
  public:
    explicit MacGamepadBackend(QObject* parent = nullptr) : GamepadBackend(parent) {
        center_ = [NSNotificationCenter defaultCenter];
        connectObs_ = [center_ addObserverForName:GCControllerDidConnectNotification
                                           object:nil
                                            queue:[NSOperationQueue mainQueue]
                                       usingBlock:^(NSNotification*) {
                                         attachFirst();
                                       }];
        disconnectObs_ = [center_ addObserverForName:GCControllerDidDisconnectNotification
                                              object:nil
                                               queue:[NSOperationQueue mainQueue]
                                          usingBlock:^(NSNotification*) {
                                            attachFirst();
                                          }];
        attachFirst();
#if TARGET_OS_OSX
        hidManager_ = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        IOHIDManagerSetDeviceMatching(hidManager_, nullptr);
        IOHIDManagerRegisterDeviceMatchingCallback(hidManager_, hidDeviceMatched, this);
        IOHIDManagerRegisterDeviceRemovalCallback(hidManager_, hidDeviceRemoved, this);
        IOHIDManagerRegisterInputValueCallback(hidManager_, hidValueChanged, this);
        IOHIDManagerScheduleWithRunLoop(hidManager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        IOHIDManagerOpen(hidManager_, kIOHIDOptionsTypeNone);
#endif
    }
    ~MacGamepadBackend() override {
        if (controller_.extendedGamepad)
            controller_.extendedGamepad.valueChangedHandler = nil;
        if (connectObs_)
            [center_ removeObserver:connectObs_];
        if (disconnectObs_)
            [center_ removeObserver:disconnectObs_];
#if TARGET_OS_OSX
        if (hidManager_) {
            IOHIDManagerUnscheduleFromRunLoop(hidManager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
            IOHIDManagerClose(hidManager_, kIOHIDOptionsTypeNone);
            CFRelease(hidManager_);
        }
#endif
    }

  private:
    void update(GCExtendedGamepad* p) {
        if (!p) {
            publish(0);
            return;
        }
        quint32 m = 0;
        const float x = p.leftThumbstick.xAxis.value;
        const float y = p.leftThumbstick.yAxis.value;
        if (p.dpad.left.isPressed || x < -0.45f)
            m |= prg32qt::input::Left;
        if (p.dpad.right.isPressed || x > 0.45f)
            m |= prg32qt::input::Right;
        if (p.dpad.up.isPressed || y > 0.45f)
            m |= prg32qt::input::Up;
        if (p.dpad.down.isPressed || y < -0.45f)
            m |= prg32qt::input::Down;
        if (p.buttonA.isPressed)
            m |= prg32qt::input::A;
        if (p.buttonB.isPressed)
            m |= prg32qt::input::B;
        if (@available(macOS 10.15, iOS 13.0, *)) {
            if (p.buttonMenu.isPressed || p.buttonOptions.isPressed)
                m |= prg32qt::input::Select;
        }
        publish(m);
    }
    void attachFirst() {
        if (controller_.extendedGamepad)
            controller_.extendedGamepad.valueChangedHandler = nil;
        NSArray<GCController*>* controllers = [GCController controllers];
        controller_ = controllers.count ? controllers.firstObject : nil;
        if (!controller_) {
#if TARGET_OS_OSX
            publishHidState();
#else
            setState(false, {});
            publish(0);
#endif
            return;
        }
        NSString* vendor = controller_.vendorName ?: @"Game Controller";
        setState(true, QString::fromUtf8([vendor UTF8String]));
        GCExtendedGamepad* pad = controller_.extendedGamepad;
        if (!pad) {
            publish(0);
            return;
        }
        pad.valueChangedHandler = ^(GCExtendedGamepad* gamepad, GCControllerElement*) {
          update(gamepad);
        };
        update(pad);
    }
#if TARGET_OS_OSX
    static void hidDeviceMatched(void* context, IOReturn, void*, IOHIDDeviceRef device) {
        auto* backend = static_cast<MacGamepadBackend*>(context);
        if (!IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick) &&
            !IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad))
            return;
        backend->hidDevice_ = device;
        backend->publishHidState();
    }
    static void hidDeviceRemoved(void* context, IOReturn, void*, IOHIDDeviceRef device) {
        auto* backend = static_cast<MacGamepadBackend*>(context);
        if (backend->hidDevice_ != device)
            return;
        backend->hidDevice_ = nullptr;
        backend->hidAxes_.fill(0);
        backend->hidButtons_.fill(false);
        backend->publishHidState();
    }
    static void hidValueChanged(void* context, IOReturn, void*, IOHIDValueRef value) {
        auto* backend = static_cast<MacGamepadBackend*>(context);
        if (backend->controller_ || IOHIDValueGetElement(value) == nullptr)
            return;
        IOHIDElementRef element = IOHIDValueGetElement(value);
        if (IOHIDElementGetDevice(element) != backend->hidDevice_)
            return;
        uint32_t page = IOHIDElementGetUsagePage(element);
        uint32_t usage = IOHIDElementGetUsage(element);
        CFIndex logicalMinimum = IOHIDElementGetLogicalMin(element);
        CFIndex logicalMaximum = IOHIDElementGetLogicalMax(element);
        CFIndex raw = IOHIDValueGetIntegerValue(value);
        double normalized =
            logicalMaximum == logicalMinimum
                ? 0.0
                : (double(raw - logicalMinimum) / double(logicalMaximum - logicalMinimum)) * 2.0 - 1.0;
        if (page == kHIDPage_GenericDesktop && usage == kHIDUsage_GD_X)
            backend->hidAxes_[0] = normalized;
        else if (page == kHIDPage_GenericDesktop && usage == kHIDUsage_GD_Y)
            backend->hidAxes_[1] = normalized;
        else if (page == kHIDPage_GenericDesktop && usage == kHIDUsage_GD_Hatswitch)
            backend->hidHat_ = int(raw);
        else if (page == kHIDPage_Button && usage > 0 && usage <= backend->hidButtons_.size())
            backend->hidButtons_[usage - 1] = raw != 0;
        backend->publishHidState();
    }
    void publishHidState() {
        if (controller_)
            return;
        if (!hidDevice_) {
            setState(false, {});
            publish(0);
            return;
        }
        QString name = "USB game controller";
        if (auto product =
                static_cast<CFStringRef>(IOHIDDeviceGetProperty(hidDevice_, CFSTR(kIOHIDProductKey)))) {
            char buffer[256] = {};
            if (CFStringGetCString(product, buffer, sizeof(buffer), kCFStringEncodingUTF8))
                name = QString::fromUtf8(buffer);
        }
        setState(true, name);
        quint32 mask = 0;
        if (hidAxes_[0] < -0.45 || hidHat_ == 5 || hidHat_ == 6 || hidHat_ == 7)
            mask |= prg32qt::input::Left;
        if (hidAxes_[0] > 0.45 || hidHat_ == 1 || hidHat_ == 2 || hidHat_ == 3)
            mask |= prg32qt::input::Right;
        if (hidAxes_[1] < -0.45 || hidHat_ == 7 || hidHat_ == 0 || hidHat_ == 1)
            mask |= prg32qt::input::Up;
        if (hidAxes_[1] > 0.45 || hidHat_ == 3 || hidHat_ == 4 || hidHat_ == 5)
            mask |= prg32qt::input::Down;
        if (hidButtons_[0])
            mask |= prg32qt::input::A;
        if (hidButtons_[1])
            mask |= prg32qt::input::B;
        if (hidButtons_[6] || hidButtons_[7] || hidButtons_[8] || hidButtons_[9])
            mask |= prg32qt::input::Select;
        publish(mask);
    }
#endif
    NSNotificationCenter* center_ = nil;
    id connectObs_ = nil;
    id disconnectObs_ = nil;
    GCController* controller_ = nil;
#if TARGET_OS_OSX
    IOHIDManagerRef hidManager_ = nullptr;
    IOHIDDeviceRef hidDevice_ = nullptr;
    std::array<double, 2> hidAxes_{};
    std::array<bool, 32> hidButtons_{};
    int hidHat_ = -1;
#endif
};
} // namespace
GamepadBackend* createGamepadBackend(QObject* parent) {
    return new MacGamepadBackend(parent);
}
