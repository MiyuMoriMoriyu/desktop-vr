#include "platform.h"

#include <poll.h>

#include "session_linux.h"

namespace
{
float g_refreshRate = 60.f;
}

bool platform::StartSession()
{
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        return false;
    }
    sockaddr_un address;
    const socklen_t length = session::Address(&address);
    bool active = false;
    if (connect(fd, reinterpret_cast<const sockaddr*>(&address), length) == 0 && session::SameUser(fd))
    {
        pollfd ready{ fd, POLLIN, 0 };
        session::Message hello{};
        if (poll(&ready, 1, 2000) > 0 && recv(fd, &hello, sizeof(hello), MSG_WAITALL) == sizeof(hello) &&
            hello.type == session::MessageType::Hello)
        {
            active = true;
            if (hello.x > 1.f)
            {
                g_refreshRate = hello.x;
            }
        }
    }
    close(fd);
    return active;
}

float platform::RefreshRate()
{
    return g_refreshRate;
}

void platform::WindowPosition(uint32_t, uint32_t, int32_t* x, int32_t* y)
{
    *x = 0;
    *y = 0;
}
