#include "rig.h"

#include <algorithm>

namespace
{
constexpr double kFocusDistance = 1.5;
constexpr double kWheelStep = 0.04;
constexpr double kRaiseHold = 5.0;
const Vec3 kUp{ 0.0, 1.0, 0.0 };
const Vec3 kRight{ 1.0, 0.0, 0.0 };
const Vec3 kForward{ 0.0, 0.0, -1.0 };
const Vec3 kRestDirection = Normalize({ 0.0, -1.0, -0.18 });
const Vec3 kDefaultOffsets[2] = { { -0.18, -0.28, -0.35 }, { 0.18, -0.28, -0.35 } };
const Vec3 kRestOffsets[2] = { { -0.2, -0.7, 0.02 }, { 0.2, -0.7, 0.02 } };

double Approach(double current, double target, double rate, double dt)
{
    return current + (target - current) * std::min(1.0, dt * rate);
}

double SmoothStep(double t)
{
    return t * t * (3.0 - 2.0 * t);
}
}

vr::DriverPose_t ToDriverPose(const Pose& pose)
{
    vr::DriverPose_t result{};
    result.qWorldFromDriverRotation.w = 1.0;
    result.qDriverFromHeadRotation.w = 1.0;
    result.qRotation = pose.rotation;
    result.vecPosition[0] = pose.position.x;
    result.vecPosition[1] = pose.position.y;
    result.vecPosition[2] = pose.position.z;
    result.poseIsValid = true;
    result.deviceIsConnected = true;
    result.result = vr::TrackingResult_Running_OK;
    return result;
}

Rig::Rig(const Settings& settings)
    : settings_(settings)
    , height_(settings.standingHeight)
{
    ResetHands();
    Update({}, 0.0);
}

void Rig::ResetHands()
{
    offsets_[0] = kDefaultOffsets[0];
    offsets_[1] = kDefaultOffsets[1];
}

void Rig::Update(const InputFrame& input, double dt)
{
    time_ += dt;
    if (input.resetHands)
    {
        ResetHands();
    }

    if (input.movingHand < 0)
    {
        const double step = Rad(settings_.sensitivity);
        yaw_ -= input.lookX * step;
        pitch_ = std::clamp(pitch_ - input.lookY * step, Rad(-85.0), Rad(85.0));
    }
    else
    {
        Vec3& offset = offsets_[input.movingHand];
        offset.x = std::clamp(offset.x + input.lookX * settings_.handSpeed, -0.7, 0.7);
        offset.y = std::clamp(offset.y - input.lookY * settings_.handSpeed, -0.8, 0.4);
    }

    const int wheelHand = input.movingHand == 0 ? 0 : 1;
    if (input.wheel != 0.0)
    {
        Vec3& offset = offsets_[wheelHand];
        offset.z = std::clamp(offset.z - input.wheel * kWheelStep, -0.75, -0.1);
    }

    const double target = input.prone ? settings_.proneHeight : input.crouch ? settings_.crouchHeight : settings_.standingHeight;
    height_ = Approach(height_, target, 8.0, dt);
    bodyYaw_ = Approach(bodyYaw_, yaw_, 3.0, dt);

    const double breath = std::sin(time_ * 2.0 * kPi / 4.5);
    head_.rotation = AxisAngle(kUp, yaw_) * AxisAngle(kRight, pitch_);
    head_.position = { 0.0, height_ + breath * 0.004, 0.0 };

    const Quat body = AxisAngle(kUp, bodyYaw_);
    const Vec3 up = Rotate(head_.rotation, kUp);
    const Vec3 focus = head_.position + Rotate(head_.rotation, { 0.0, 0.0, -kFocusDistance });
    const Quat aim = AxisAngle(kRight, Rad(settings_.aimPitch));
    const Quat restRotation = LookRotation(Rotate(body, kRestDirection), Rotate(body, kForward)) * aim;

    for (int hand = 0; hand < 2; ++hand)
    {
        const HandInput& handInput = input.hands[hand];
        const bool used = handInput.trigger || handInput.grip || input.hands[0].b || input.movingHand == hand ||
            (input.wheel != 0.0 && wheelHand == hand);
        idle_[hand] = used ? 0.0 : idle_[hand] + dt;
        const double raise = idle_[hand] < kRaiseHold ? 1.0 : 0.0;
        readiness_[hand] = Approach(readiness_[hand], raise, raise > readiness_[hand] ? 10.0 : 3.0, dt);
        const double blend = SmoothStep(std::clamp(readiness_[hand], 0.0, 1.0));

        const Vec3 readyPosition = head_.position + Rotate(head_.rotation, offsets_[hand]);
        const Quat readyRotation = LookRotation(focus - readyPosition, up) * aim;

        const double phase = time_ * 0.9 + hand * 1.7;
        const Vec3 sway{ std::sin(phase) * 0.008, 0.0, std::cos(phase * 0.8) * 0.01 };
        Vec3 restPosition = head_.position + Rotate(body, kRestOffsets[hand] + sway);
        restPosition.y = std::max(restPosition.y, 0.05);

        hands_[hand].position = Lerp(restPosition, readyPosition, blend);
        hands_[hand].rotation = Slerp(restRotation, readyRotation, blend);
    }
}
