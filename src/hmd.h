#pragma once

#include <atomic>
#include <mutex>

#include "openvr_driver.h"
#include "rig.h"
#include "settings.h"

class Hmd final : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent
{
public:
    explicit Hmd(const Settings& settings);

    vr::EVRInitError Activate(uint32_t objectId) override;
    void Deactivate() override;
    void EnterStandby() override {}
    void* GetComponent(const char* componentNameAndVersion) override;
    void DebugRequest(const char* request, char* responseBuffer, uint32_t responseBufferSize) override;
    vr::DriverPose_t GetPose() override;

    void GetWindowBounds(int32_t* x, int32_t* y, uint32_t* width, uint32_t* height) override;
    bool IsDisplayOnDesktop() override;
    bool IsDisplayRealDisplay() override { return false; }
    void GetRecommendedRenderTargetSize(uint32_t* width, uint32_t* height) override;
    void GetEyeOutputViewport(vr::EVREye eye, uint32_t* x, uint32_t* y, uint32_t* width, uint32_t* height) override;
    void GetProjectionRaw(vr::EVREye eye, float* left, float* right, float* top, float* bottom) override;
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eye, float u, float v) override;
    bool ComputeInverseDistortion(vr::HmdVector2_t*, vr::EVREye, uint32_t, float, float) override { return false; }

    void Publish(const Pose& head);

private:
    Settings settings_;
    std::atomic<uint32_t> objectId_{ vr::k_unTrackedDeviceIndexInvalid };
    std::mutex poseLock_;
    vr::DriverPose_t pose_{};
};
