#include "hmd.h"

#include <windows.h>

#include <cmath>
#include <cstring>

namespace
{
float RefreshRate()
{
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1)
    {
        return static_cast<float>(mode.dmDisplayFrequency);
    }
    return 60.f;
}
}

Hmd::Hmd(const Settings& settings)
    : settings_(settings)
    , pose_(ToDriverPose({}))
{
}

vr::EVRInitError Hmd::Activate(uint32_t objectId)
{
    const vr::PropertyContainerHandle_t container = vr::VRProperties()->TrackedDeviceToPropertyContainer(objectId);
    vr::VRProperties()->SetStringProperty(container, vr::Prop_ModelNumber_String, "Desktop VR");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_ManufacturerName_String, "Desktop VR");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_TrackingSystemName_String, "desktopvr");
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserIpdMeters_Float, 0.f);
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.f);
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_DisplayFrequency_Float, RefreshRate());
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_SecondsFromVsyncToPhotons_Float, 0.01f);
    vr::VRProperties()->SetBoolProperty(container, vr::Prop_IsOnDesktop_Bool, false);
    vr::VRProperties()->SetBoolProperty(container, vr::Prop_DisplayDebugMode_Bool, true);
    vr::VRProperties()->SetBoolProperty(container, vr::Prop_ContainsProximitySensor_Bool, true);

    vr::VRInputComponentHandle_t proximity;
    vr::VRDriverInput()->CreateBooleanComponent(container, "/proximity", &proximity);
    vr::VRDriverInput()->UpdateBooleanComponent(proximity, true, 0.0);
    objectId_ = objectId;
    return vr::VRInitError_None;
}

void Hmd::Deactivate()
{
    objectId_ = vr::k_unTrackedDeviceIndexInvalid;
}

void* Hmd::GetComponent(const char* componentNameAndVersion)
{
    if (std::strcmp(componentNameAndVersion, vr::IVRDisplayComponent_Version) == 0)
    {
        return static_cast<vr::IVRDisplayComponent*>(this);
    }
    return nullptr;
}

void Hmd::DebugRequest(const char*, char* responseBuffer, uint32_t responseBufferSize)
{
    if (responseBufferSize >= 1)
    {
        responseBuffer[0] = 0;
    }
}

vr::DriverPose_t Hmd::GetPose()
{
    std::lock_guard<std::mutex> lock(poseLock_);
    return pose_;
}

void Hmd::Publish(const Pose& head)
{
    const vr::DriverPose_t pose = ToDriverPose(head);
    {
        std::lock_guard<std::mutex> lock(poseLock_);
        pose_ = pose;
    }
    const uint32_t objectId = objectId_;
    if (objectId != vr::k_unTrackedDeviceIndexInvalid)
    {
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(objectId, pose, sizeof(pose));
    }
}

void Hmd::GetWindowBounds(int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
{
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    *x = workArea.right - settings_.windowWidth;
    *y = workArea.bottom - settings_.windowHeight;
    *width = settings_.windowWidth;
    *height = settings_.windowHeight;
}

void Hmd::GetRecommendedRenderTargetSize(uint32_t* width, uint32_t* height)
{
    *width = settings_.renderWidth;
    *height = settings_.renderHeight;
}

void Hmd::GetEyeOutputViewport(vr::EVREye, uint32_t* x, uint32_t* y, uint32_t* width, uint32_t* height)
{
    *x = 0;
    *y = 0;
    *width = settings_.windowWidth;
    *height = settings_.windowHeight;
}

void Hmd::GetProjectionRaw(vr::EVREye, float* left, float* right, float* top, float* bottom)
{
    const double tanVertical = std::tan(Rad(settings_.fovVertical) * 0.5);
    const double tanHorizontal = tanVertical * settings_.renderWidth / settings_.renderHeight;
    *left = static_cast<float>(-tanHorizontal);
    *right = static_cast<float>(tanHorizontal);
    *top = static_cast<float>(-tanVertical);
    *bottom = static_cast<float>(tanVertical);
}

vr::DistortionCoordinates_t Hmd::ComputeDistortion(vr::EVREye, float u, float v)
{
    vr::DistortionCoordinates_t coordinates{};
    coordinates.rfRed[0] = coordinates.rfGreen[0] = coordinates.rfBlue[0] = u;
    coordinates.rfRed[1] = coordinates.rfGreen[1] = coordinates.rfBlue[1] = v;
    return coordinates;
}
