#pragma once

struct Settings
{
    int windowWidth = 0;
    int windowHeight = 0;
    int renderWidth = 0;
    int renderHeight = 0;
    double fovVertical = 0.0;
    double sensitivity = 0.0;
    double handSpeed = 0.0;
    double rotateSpeed = 0.0;
    double standingHeight = 0.0;
    double crouchHeight = 0.0;
    double proneHeight = 0.0;
    double aimPitch = 0.0;
};

Settings LoadSettings();
