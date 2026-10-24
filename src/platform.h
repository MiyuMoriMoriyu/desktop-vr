#pragma once

#include <cstdint>

namespace platform
{
bool StartSession();
float RefreshRate();
void WindowPosition(uint32_t width, uint32_t height, int32_t* x, int32_t* y);
}
