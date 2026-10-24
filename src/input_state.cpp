#include "input_state.h"

#include <utility>

void InputState::SetKey(Key key, bool down)
{
    std::lock_guard<std::mutex> lock(lock_);
    bool& held = keys_[static_cast<int>(key)];
    const bool pressed = down && !held;
    held = down;
    if (pressed)
    {
        OnPress(key);
    }
}

void InputState::SetButton(MouseButton button, bool down)
{
    std::lock_guard<std::mutex> lock(lock_);
    buttons_[static_cast<int>(button)] = down;
}

void InputState::AddMotion(double dx, double dy)
{
    std::lock_guard<std::mutex> lock(lock_);
    dx_ += dx;
    dy_ += dy;
}

void InputState::AddWheel(double steps)
{
    std::lock_guard<std::mutex> lock(lock_);
    wheel_ += steps;
}

void InputState::Release()
{
    std::lock_guard<std::mutex> lock(lock_);
    keys_.fill(false);
    buttons_.fill(false);
    dx_ = 0.0;
    dy_ = 0.0;
    wheel_ = 0.0;
}

void InputState::OnPress(Key key)
{
    if (key >= Key::F1 && key <= Key::F8)
    {
        const int gesture = static_cast<int>(key) - static_cast<int>(Key::F1);
        const bool leftShift = Held(Key::LeftShift);
        const bool rightShift = Held(Key::RightShift);
        if (leftShift || !rightShift)
        {
            gestures_[0] = gesture;
        }
        if (rightShift || !leftShift)
        {
            gestures_[1] = gesture;
        }
    }
    else if (key == Key::C)
    {
        crouch_ = !crouch_;
        prone_ = false;
    }
    else if (key == Key::Z)
    {
        prone_ = !prone_;
        crouch_ = false;
    }
    else if (key == Key::R)
    {
        resetHands_ = true;
    }
}

InputFrame InputState::Poll(bool captured)
{
    std::lock_guard<std::mutex> lock(lock_);
    InputFrame frame;
    frame.captured = captured;
    frame.crouch = crouch_;
    frame.prone = prone_;
    frame.resetHands = std::exchange(resetHands_, false);
    frame.hands[0].gesture = gestures_[0];
    frame.hands[1].gesture = gestures_[1];

    const double dx = std::exchange(dx_, 0.0);
    const double dy = std::exchange(dy_, 0.0);
    const double wheel = std::exchange(wheel_, 0.0);
    if (!captured)
    {
        return frame;
    }

    frame.lookX = dx;
    frame.lookY = dy;
    frame.wheel = wheel;

    frame.movingHand = Held(Key::Q) ? 0 : (Held(Key::E) ? 1 : -1);
    frame.rotatingHand = Held(Key::One) ? 0 : (Held(Key::Three) ? 1 : -1);
    const int selected = frame.rotatingHand >= 0 ? frame.rotatingHand : frame.movingHand;
    HandInput& active = frame.hands[selected == 0 ? 0 : 1];
    active.trigger = buttons_[static_cast<int>(MouseButton::Left)];
    active.grip = buttons_[static_cast<int>(MouseButton::Right)];

    float moveX = static_cast<float>(Held(Key::D)) - static_cast<float>(Held(Key::A));
    float moveY = static_cast<float>(Held(Key::W)) - static_cast<float>(Held(Key::S));
    if (moveX != 0.f && moveY != 0.f)
    {
        moveX *= 0.7071f;
        moveY *= 0.7071f;
    }
    frame.hands[0].stickX = moveX;
    frame.hands[0].stickY = moveY;
    frame.hands[1].stickX = static_cast<float>(Held(Key::Right)) - static_cast<float>(Held(Key::Left));

    frame.hands[0].a = Held(Key::V);
    frame.hands[0].b = Held(Key::Escape);
    frame.hands[1].a = Held(Key::Space);
    return frame;
}
