#include "GamepadBackend.h"
GamepadBackend *createGamepadBackend(QObject *parent) { return new GamepadBackend(parent); }
