#pragma once

#include <array>
#include <mutex>

#include "input.h"

enum class Key
{
    W,
    A,
    S,
    D,
    Q,
    E,
    R,
    C,
    Z,
    V,
    One,
    Three,
    Space,
    Escape,
    Left,
    Right,
    LeftShift,
    RightShift,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    Count
};

enum class MouseButton
{
    Left,
    Right,
    Count
};

class InputState
{
public:
    void SetKey(Key key, bool down);
    void SetButton(MouseButton button, bool down);
    void AddMotion(double dx, double dy);
    void AddWheel(double steps);
    void Release();
    InputFrame Poll(bool captured);

private:
    bool Held(Key key) const { return keys_[static_cast<int>(key)]; }
    void OnPress(Key key);

    std::mutex lock_;
    std::array<bool, static_cast<int>(Key::Count)> keys_{};
    std::array<bool, static_cast<int>(MouseButton::Count)> buttons_{};
    double dx_ = 0.0;
    double dy_ = 0.0;
    double wheel_ = 0.0;
    bool crouch_ = false;
    bool prone_ = false;
    bool resetHands_ = false;
    std::array<int, 2> gestures_{};
};
