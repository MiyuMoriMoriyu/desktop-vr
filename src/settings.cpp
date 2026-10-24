#include "settings.h"

#include "openvr_driver.h"

namespace
{
const char kSection[] = "driver_desktopvr";

int Int(const char* key)
{
    return vr::VRSettings()->GetInt32(kSection, key);
}

double Float(const char* key)
{
    return vr::VRSettings()->GetFloat(kSection, key);
}
}

Settings LoadSettings()
{
    Settings settings;
    settings.windowWidth = Int("window_width");
    settings.windowHeight = Int("window_height");
    settings.renderWidth = Int("render_width");
    settings.renderHeight = Int("render_height");
    settings.fovVertical = Float("fov_vertical");
    settings.sensitivity = Float("mouse_sensitivity");
    settings.handSpeed = Float("hand_speed");
    settings.rotateSpeed = Float("rotate_speed");
    settings.standingHeight = Float("standing_height");
    settings.crouchHeight = Float("crouch_height");
    settings.proneHeight = Float("prone_height");
    settings.aimPitch = Float("aim_pitch");
    return settings;
}
