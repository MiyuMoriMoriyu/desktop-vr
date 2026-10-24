#include "platform.h"

#include <windows.h>

bool platform::StartSession()
{
    HANDLE session = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\DesktopVRSession");
    if (!session)
    {
        return false;
    }
    CloseHandle(session);
    return true;
}

float platform::RefreshRate()
{
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1)
    {
        return static_cast<float>(mode.dmDisplayFrequency);
    }
    return 60.f;
}

void platform::WindowPosition(uint32_t width, uint32_t height, int32_t* x, int32_t* y)
{
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    *x = workArea.right - static_cast<int32_t>(width);
    *y = workArea.bottom - static_cast<int32_t>(height);
}
