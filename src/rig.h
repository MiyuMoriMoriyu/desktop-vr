#pragma once

#include "input.h"
#include "settings.h"
#include "vecmath.h"

struct Pose
{
    Vec3 position;
    Quat rotation{ 1.0, 0.0, 0.0, 0.0 };
};

vr::DriverPose_t ToDriverPose(const Pose& pose);

class Rig
{
public:
    explicit Rig(const Settings& settings);

    void Update(const InputFrame& input, double dt);
    const Pose& Head() const { return head_; }
    const Pose& Hand(int hand) const { return hands_[hand]; }

private:
    void ResetHands();

    Settings settings_;
    double yaw_ = 0.0;
    double pitch_ = 0.0;
    double height_ = 0.0;
    double bodyYaw_ = 0.0;
    double time_ = 0.0;
    double idle_[2] = { 1e9, 1e9 };
    double readiness_[2] = { 0.0, 0.0 };
    Vec3 offsets_[2];
    Pose head_;
    Pose hands_[2];
};
