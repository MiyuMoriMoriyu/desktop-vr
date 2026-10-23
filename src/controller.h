#pragma once

#include <atomic>
#include <mutex>

#include "hand.h"
#include "input.h"
#include "openvr_driver.h"
#include "rig.h"

class Controller final : public vr::ITrackedDeviceServerDriver
{
public:
    explicit Controller(bool rightHand);

    const char* Serial() const;

    vr::EVRInitError Activate(uint32_t objectId) override;
    void Deactivate() override;
    void EnterStandby() override {}
    void* GetComponent(const char*) override { return nullptr; }
    void DebugRequest(const char* request, char* responseBuffer, uint32_t responseBufferSize) override;
    vr::DriverPose_t GetPose() override;

    void Publish(const Pose& pose, const HandInput& input, double dt);

private:
    enum Button
    {
        ATouch,
        AClick,
        BTouch,
        BClick,
        TriggerTouch,
        TriggerClick,
        GripTouch,
        StickTouch,
        ButtonCount
    };

    enum Axis
    {
        TriggerValue,
        GripValue,
        GripForce,
        StickX,
        StickY,
        FingerIndex,
        FingerMiddle,
        FingerRing,
        FingerPinky,
        AxisCount
    };

    void SetButton(Button button, bool value);
    void SetAxis(Axis axis, float value);
    void UpdateSkeleton();

    const bool rightHand_;
    std::atomic<uint32_t> objectId_{ vr::k_unTrackedDeviceIndexInvalid };
    std::mutex poseLock_;
    vr::DriverPose_t pose_{};
    vr::VRInputComponentHandle_t buttons_[ButtonCount]{};
    vr::VRInputComponentHandle_t axes_[AxisCount]{};
    vr::VRInputComponentHandle_t skeleton_ = vr::k_ulInvalidInputComponentHandle;
    bool buttonState_[ButtonCount]{};
    float axisState_[AxisCount]{};
    FingerCurls curls_;
};
