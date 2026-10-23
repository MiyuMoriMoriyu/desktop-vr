// Adapted from the OpenVR handskeletonsimulation sample. Copyright (c) Valve Corporation, BSD-3-Clause.
#include "hand.h"

#include <utility>

#include "vecmath.h"

namespace
{
struct Joint
{
    double swingX = 0.0;
    double swingY = 0.0;
    double twist = 0.0;
};

struct Finger
{
    Joint metacarpal;
    Joint proximal;
    double intermediate = 0.0;
    double distal = 0.0;
};

const double kLengths[5][5] = {
    { 0.05, 0.05, 0.035, 0.025, 0.0 },
    { 0.03, 0.073, 0.045, 0.025, 0.02 },
    { 0.01, 0.091, 0.049, 0.03, 0.02 },
    { 0.02, 0.073, 0.045, 0.03, 0.03 },
    { 0.03, 0.067, 0.03, 0.025, 0.02 },
};

const Quat kIdentity = { 1.0, 0.0, 0.0, 0.0 };

Quat SwingTwist(const Joint& joint)
{
    const double swingSquared = joint.swingX * joint.swingX + joint.swingY * joint.swingY;
    const double cosTwist = std::cos(joint.twist * 0.5);
    const double sinTwist = std::sin(joint.twist * 0.5);
    double cosSwing = 1.0;
    double sinSwingOverTheta = 0.5;
    if (swingSquared > 0.0)
    {
        const double theta = std::sqrt(swingSquared);
        cosSwing = std::cos(theta * 0.5);
        sinSwingOverTheta = std::sin(theta * 0.5) / theta;
    }
    return {
        cosSwing * cosTwist,
        cosSwing * sinTwist,
        (joint.swingY * cosTwist - joint.swingX * sinTwist) * sinSwingOverTheta,
        (joint.swingX * cosTwist + joint.swingY * sinTwist) * sinSwingOverTheta,
    };
}

Quat Roll(double angle)
{
    return { std::cos(angle * 0.5), 0.0, 0.0, std::sin(angle * 0.5) };
}

vr::VRBoneTransform_t Bone(bool rightHand, const Quat& orientation, const Vec3& position)
{
    vr::VRBoneTransform_t bone{};
    bone.position = { static_cast<float>(rightHand ? -position.x : position.x), static_cast<float>(position.y), static_cast<float>(position.z), 1.f };
    bone.orientation = { static_cast<float>(orientation.w), static_cast<float>(orientation.x), static_cast<float>(orientation.y), static_cast<float>(orientation.z) };
    return bone;
}

vr::VRBoneTransform_t Bone(bool rightHand, const Quat& orientation, double length)
{
    return Bone(rightHand, orientation, Vec3{ length, 0.0, 0.0 });
}

vr::VRBoneTransform_t Metacarpal(bool rightHand, const Quat& orientation, double length)
{
    Quat bone = Quat{ 0.5, 0.5, -0.5, 0.5 } * orientation;
    const Vec3 position = Rotate(bone, { length, 0.0, 0.0 });
    if (rightHand)
    {
        std::swap(bone.w, bone.x);
        std::swap(bone.y, bone.z);
        bone.x = -bone.x;
        bone.z = -bone.z;
    }
    return Bone(rightHand, bone, position);
}
}

void ComputeHandSkeleton(bool rightHand, const FingerCurls& curls, vr::VRBoneTransform_t* out)
{
    out[0] = { { 0.f, 0.f, 0.f, 1.f }, { 1.f, 0.f, 0.f, 0.f } };
    out[1] = { { -0.034038f, 0.036503f, 0.164722f, 1.f }, { -0.055147f, -0.078608f, -0.920279f, 0.379296f } };
    if (rightHand)
    {
        out[1].position.v[0] = -out[1].position.v[0];
        out[1].orientation.y = -out[1].orientation.y;
        out[1].orientation.z = -out[1].orientation.z;
    }

    Joint thumbMetacarpal{ Rad(10.0 + curls.thumb * 5.0), Rad(40.0), 0.0 };
    Joint thumbProximal{ Rad(curls.thumb * 90.0), 0.0, 0.0 };
    const double thumbDistal = Rad(curls.thumb * 90.0);

    out[2] = Metacarpal(rightHand, SwingTwist(thumbMetacarpal), kLengths[0][0]);
    out[3] = Bone(rightHand, SwingTwist(thumbProximal), kLengths[0][1]);
    out[4] = Bone(rightHand, Roll(thumbDistal), kLengths[0][2]);
    out[5] = Bone(rightHand, kIdentity, kLengths[0][3]);

    const float fingerCurls[4] = { curls.index, curls.middle, curls.ring, curls.pinky };
    const double metacarpalSplay[4] = { 13.0, 0.0, -15.0, -27.0 };
    const double proximalSplay[4] = { 3.0, 0.0, -1.0, -2.0 };

    for (int f = 0; f < 4; ++f)
    {
        const double curl = fingerCurls[f];
        Finger finger;
        finger.metacarpal = { Rad(curl * 5.0), Rad(metacarpalSplay[f]), 0.0 };
        finger.proximal = { Rad(curl * 90.0), Rad(proximalSplay[f]), 0.0 };
        finger.intermediate = Rad(5.0 + curl * 80.0);
        finger.distal = Rad(5.0 + curl * 80.0);

        const int base = 6 + f * 5;
        const double* lengths = kLengths[f + 1];
        out[base] = Metacarpal(rightHand, SwingTwist(finger.metacarpal), lengths[0]);
        out[base + 1] = Bone(rightHand, SwingTwist(finger.proximal), lengths[1]);
        out[base + 2] = Bone(rightHand, Roll(finger.intermediate), lengths[2]);
        out[base + 3] = Bone(rightHand, Roll(finger.distal), lengths[3]);
        out[base + 4] = Bone(rightHand, kIdentity, lengths[4]);
    }

    for (int i = 26; i < kBoneCount; ++i)
    {
        out[i] = { { 0.f, 0.f, 0.f, 1.f }, { 1.f, 0.f, 0.f, 0.f } };
    }
}
