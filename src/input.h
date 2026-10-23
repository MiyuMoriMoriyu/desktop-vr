#pragma once

#include <array>

struct HandInput
{
    bool trigger = false;
    bool grip = false;
    bool a = false;
    bool b = false;
    float stickX = 0.f;
    float stickY = 0.f;
    int gesture = 0;
};

struct InputFrame
{
    bool captured = false;
    double lookX = 0.0;
    double lookY = 0.0;
    double wheel = 0.0;
    int movingHand = -1;
    bool crouch = false;
    bool prone = false;
    bool resetHands = false;
    std::array<HandInput, 2> hands{};
};

namespace input
{
void Start();
void Stop();
InputFrame Poll();
}
