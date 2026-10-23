#include "controller.h"

#include <algorithm>

namespace
{
const FingerCurls kGestures[] = {
    { 0.35f, 0.3f, 0.4f, 0.45f, 0.5f },
    { 1.f, 1.f, 1.f, 1.f, 1.f },
    { 0.f, 0.f, 0.f, 0.f, 0.f },
    { 1.f, 0.f, 1.f, 1.f, 1.f },
    { 1.f, 0.f, 0.f, 1.f, 1.f },
    { 1.f, 0.f, 1.f, 1.f, 0.f },
    { 0.f, 0.f, 1.f, 1.f, 1.f },
    { 0.f, 1.f, 1.f, 1.f, 1.f },
};
constexpr int kGestureCount = sizeof(kGestures) / sizeof(kGestures[0]);

float Approach(float current, float target, float blend)
{
    return current + (target - current) * blend;
}
}

Controller::Controller(bool rightHand)
    : rightHand_(rightHand)
    , pose_(ToDriverPose({}))
    , curls_(kGestures[0])
{
}

const char* Controller::Serial() const
{
    return rightHand_ ? "DesktopVR-Right" : "DesktopVR-Left";
}

vr::EVRInitError Controller::Activate(uint32_t objectId)
{
    vr::CVRPropertyHelpers* properties = vr::VRProperties();
    vr::IVRDriverInput* driverInput = vr::VRDriverInput();
    const vr::PropertyContainerHandle_t container = properties->TrackedDeviceToPropertyContainer(objectId);

    properties->SetStringProperty(container, vr::Prop_ModelNumber_String, rightHand_ ? "Knuckles Right" : "Knuckles Left");
    properties->SetStringProperty(container, vr::Prop_ManufacturerName_String, "Valve");
    properties->SetStringProperty(container, vr::Prop_TrackingSystemName_String, "desktopvr");
    properties->SetStringProperty(container, vr::Prop_RenderModelName_String,
        rightHand_ ? "{indexcontroller}valve_controller_knu_1_0_right" : "{indexcontroller}valve_controller_knu_1_0_left");
    properties->SetStringProperty(container, vr::Prop_ControllerType_String, "knuckles");
    properties->SetStringProperty(container, vr::Prop_InputProfilePath_String, "{indexcontroller}/input/index_controller_profile.json");
    properties->SetInt32Property(container, vr::Prop_ControllerRoleHint_Int32,
        rightHand_ ? vr::TrackedControllerRole_RightHand : vr::TrackedControllerRole_LeftHand);

    driverInput->CreateBooleanComponent(container, "/input/a/click", &buttons_[AClick]);
    driverInput->CreateBooleanComponent(container, "/input/b/click", &buttons_[BClick]);
    driverInput->CreateBooleanComponent(container, "/input/trigger/touch", &buttons_[TriggerTouch]);
    driverInput->CreateBooleanComponent(container, "/input/trigger/click", &buttons_[TriggerClick]);
    driverInput->CreateBooleanComponent(container, "/input/grip/touch", &buttons_[GripTouch]);

    driverInput->CreateScalarComponent(container, "/input/trigger/value", &axes_[TriggerValue], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    driverInput->CreateScalarComponent(container, "/input/grip/value", &axes_[GripValue], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    driverInput->CreateScalarComponent(container, "/input/grip/force", &axes_[GripForce], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    driverInput->CreateScalarComponent(container, "/input/thumbstick/x", &axes_[StickX], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
    driverInput->CreateScalarComponent(container, "/input/thumbstick/y", &axes_[StickY], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
    driverInput->CreateScalarComponent(container, "/input/finger/index", &axes_[FingerIndex], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    driverInput->CreateScalarComponent(container, "/input/finger/middle", &axes_[FingerMiddle], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    driverInput->CreateScalarComponent(container, "/input/finger/ring", &axes_[FingerRing], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    driverInput->CreateScalarComponent(container, "/input/finger/pinky", &axes_[FingerPinky], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);

    vr::VRInputComponentHandle_t haptic;
    driverInput->CreateHapticComponent(container, "/output/haptic", &haptic);

    driverInput->CreateSkeletonComponent(container,
        rightHand_ ? "/input/skeleton/right" : "/input/skeleton/left",
        rightHand_ ? "/skeleton/hand/right" : "/skeleton/hand/left",
        "/pose/raw", vr::VRSkeletalTracking_Partial, nullptr, 0, &skeleton_);
    UpdateSkeleton();

    objectId_ = objectId;
    return vr::VRInitError_None;
}

void Controller::Deactivate()
{
    objectId_ = vr::k_unTrackedDeviceIndexInvalid;
}

void Controller::DebugRequest(const char*, char* responseBuffer, uint32_t responseBufferSize)
{
    if (responseBufferSize >= 1)
    {
        responseBuffer[0] = 0;
    }
}

vr::DriverPose_t Controller::GetPose()
{
    std::lock_guard<std::mutex> lock(poseLock_);
    return pose_;
}

void Controller::Publish(const Pose& pose, const HandInput& input, double dt)
{
    const vr::DriverPose_t driverPose = ToDriverPose(pose);
    {
        std::lock_guard<std::mutex> lock(poseLock_);
        pose_ = driverPose;
    }
    const uint32_t objectId = objectId_;
    if (objectId == vr::k_unTrackedDeviceIndexInvalid)
    {
        return;
    }
    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(objectId, driverPose, sizeof(driverPose));

    SetButton(AClick, input.a);
    SetButton(BClick, input.b);
    SetButton(TriggerTouch, input.trigger);
    SetButton(TriggerClick, input.trigger);
    SetAxis(TriggerValue, input.trigger ? 1.f : 0.f);
    SetButton(GripTouch, input.grip);
    SetAxis(GripValue, input.grip ? 1.f : 0.f);
    SetAxis(GripForce, input.grip ? 1.f : 0.f);
    SetAxis(StickX, input.stickX);
    SetAxis(StickY, input.stickY);

    const FingerCurls& target = kGestures[std::clamp(input.gesture, 0, kGestureCount - 1)];
    const float blend = static_cast<float>(std::min(1.0, dt * 14.0));
    const FingerCurls previous = curls_;
    curls_.thumb = Approach(curls_.thumb, target.thumb, blend);
    curls_.index = Approach(curls_.index, target.index, blend);
    curls_.middle = Approach(curls_.middle, target.middle, blend);
    curls_.ring = Approach(curls_.ring, target.ring, blend);
    curls_.pinky = Approach(curls_.pinky, target.pinky, blend);
    SetAxis(FingerIndex, curls_.index);
    SetAxis(FingerMiddle, curls_.middle);
    SetAxis(FingerRing, curls_.ring);
    SetAxis(FingerPinky, curls_.pinky);

    if (previous.thumb != curls_.thumb || previous.index != curls_.index || previous.middle != curls_.middle ||
        previous.ring != curls_.ring || previous.pinky != curls_.pinky)
    {
        UpdateSkeleton();
    }
}

void Controller::SetButton(Button button, bool value)
{
    if (buttonState_[button] != value)
    {
        buttonState_[button] = value;
        vr::VRDriverInput()->UpdateBooleanComponent(buttons_[button], value, 0.0);
    }
}

void Controller::SetAxis(Axis axis, float value)
{
    if (axisState_[axis] != value)
    {
        axisState_[axis] = value;
        vr::VRDriverInput()->UpdateScalarComponent(axes_[axis], value, 0.0);
    }
}

void Controller::UpdateSkeleton()
{
    vr::VRBoneTransform_t bones[kBoneCount];
    ComputeHandSkeleton(rightHand_, curls_, bones);
    vr::VRDriverInput()->UpdateSkeletonComponent(skeleton_, vr::VRSkeletalMotionRange_WithController, bones, kBoneCount);
    vr::VRDriverInput()->UpdateSkeletonComponent(skeleton_, vr::VRSkeletalMotionRange_WithoutController, bones, kBoneCount);
}
