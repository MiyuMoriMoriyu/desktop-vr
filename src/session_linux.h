#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace session
{
enum class MessageType : uint8_t
{
    Hello,
    Capture,
    Key,
    Button,
    Motion,
    Wheel
};

struct Message
{
    MessageType type;
    uint8_t code;
    uint8_t down;
    uint8_t reserved;
    float x;
    float y;
};
static_assert(sizeof(Message) == 12, "unexpected message size");

inline socklen_t Address(sockaddr_un* address)
{
    std::memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    const int length = std::snprintf(address->sun_path + 1, sizeof(address->sun_path) - 1, "desktopvr-%u", static_cast<unsigned>(getuid()));
    return static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + length);
}

inline bool SameUser(int fd)
{
    ucred peer{};
    socklen_t size = sizeof(peer);
    return getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0 && peer.uid == getuid();
}
}
