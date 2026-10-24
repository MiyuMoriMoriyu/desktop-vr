#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

#include "controller.h"
#include "hmd.h"
#include "input.h"
#include "openvr_driver.h"
#include "platform.h"
#include "rig.h"
#include "settings.h"

namespace
{
class Provider final : public vr::IServerTrackedDeviceProvider
{
public:
    vr::EVRInitError Init(vr::IVRDriverContext* context) override
    {
        VR_INIT_SERVER_DRIVER_CONTEXT(context);
        if (!platform::StartSession())
        {
            return vr::VRInitError_Init_HmdNotFound;
        }

        const Settings settings = LoadSettings();
        hmd_ = std::make_unique<Hmd>(settings);
        controllers_[0] = std::make_unique<Controller>(false);
        controllers_[1] = std::make_unique<Controller>(true);
        rig_ = std::make_unique<Rig>(settings);

        vr::IVRServerDriverHost* host = vr::VRServerDriverHost();
        if (!host->TrackedDeviceAdded("DesktopVR-HMD", vr::TrackedDeviceClass_HMD, hmd_.get()))
        {
            return vr::VRInitError_Driver_Failed;
        }
        for (auto& controller : controllers_)
        {
            if (!host->TrackedDeviceAdded(controller->Serial(), vr::TrackedDeviceClass_Controller, controller.get()))
            {
                return vr::VRInitError_Driver_Failed;
            }
        }

        input::Start();
        running_ = true;
        thread_ = std::thread(&Provider::Tick, this);
        return vr::VRInitError_None;
    }

    void Cleanup() override
    {
        if (running_.exchange(false))
        {
            thread_.join();
            input::Stop();
        }
        VR_CLEANUP_SERVER_DRIVER_CONTEXT();
    }

    const char* const* GetInterfaceVersions() override { return vr::k_InterfaceVersions; }
    void RunFrame() override {}
    bool ShouldBlockStandbyMode() override { return false; }
    void EnterStandby() override {}
    void LeaveStandby() override {}

private:
    void Tick()
    {
        auto last = std::chrono::steady_clock::now();
        while (running_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            const auto now = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(now - last).count();
            last = now;

            const InputFrame frame = input::Poll();
            rig_->Update(frame, dt);
            hmd_->Publish(rig_->Head());
            for (int hand = 0; hand < 2; ++hand)
            {
                controllers_[hand]->Publish(rig_->Hand(hand), frame.hands[hand], dt);
            }
        }
    }

    std::unique_ptr<Hmd> hmd_;
    std::unique_ptr<Controller> controllers_[2];
    std::unique_ptr<Rig> rig_;
    std::atomic<bool> running_{ false };
    std::thread thread_;
};

Provider g_provider;
}

#if defined(_WIN32)
#define DESKTOPVR_EXPORT extern "C" __declspec(dllexport)
#else
#define DESKTOPVR_EXPORT extern "C" __attribute__((visibility("default")))
#endif

DESKTOPVR_EXPORT void* HmdDriverFactory(const char* interfaceName, int* returnCode)
{
    if (std::strcmp(interfaceName, vr::IServerTrackedDeviceProvider_Version) == 0)
    {
        return &g_provider;
    }
    if (returnCode)
    {
        *returnCode = vr::VRInitError_Init_InterfaceNotFound;
    }
    return nullptr;
}
