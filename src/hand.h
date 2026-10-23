#pragma once

#include "openvr_driver.h"

constexpr int kBoneCount = 31;

struct FingerCurls
{
    float thumb = 0.f;
    float index = 0.f;
    float middle = 0.f;
    float ring = 0.f;
    float pinky = 0.f;
};

void ComputeHandSkeleton(bool rightHand, const FingerCurls& curls, vr::VRBoneTransform_t* out);
