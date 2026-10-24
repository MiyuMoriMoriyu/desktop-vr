#include "input.h"
#include "input_state.h"

#include <poll.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <thread>

#include "session_linux.h"

namespace
{
InputState g_state;
std::thread g_thread;
std::atomic<bool> g_running{ false };
std::atomic<bool> g_captured{ false };

int Connect()
{
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        return -1;
    }
    sockaddr_un address;
    const socklen_t length = session::Address(&address);
    if (connect(fd, reinterpret_cast<const sockaddr*>(&address), length) == 0 && session::SameUser(fd))
    {
        return fd;
    }
    close(fd);
    return -1;
}

void Apply(const session::Message& message)
{
    switch (message.type)
    {
    case session::MessageType::Capture:
        g_captured = message.down != 0;
        if (!g_captured)
        {
            g_state.Release();
        }
        break;
    case session::MessageType::Key:
        if (message.code < static_cast<uint8_t>(Key::Count))
        {
            g_state.SetKey(static_cast<Key>(message.code), message.down != 0);
        }
        break;
    case session::MessageType::Button:
        if (message.code < static_cast<uint8_t>(MouseButton::Count))
        {
            g_state.SetButton(static_cast<MouseButton>(message.code), message.down != 0);
        }
        break;
    case session::MessageType::Motion:
        g_state.AddMotion(message.x, message.y);
        break;
    case session::MessageType::Wheel:
        g_state.AddWheel(message.x);
        break;
    default:
        break;
    }
}

void Receive(int fd)
{
    unsigned char buffer[sizeof(session::Message) * 64];
    size_t filled = 0;
    while (g_running)
    {
        pollfd ready{ fd, POLLIN, 0 };
        const int result = poll(&ready, 1, 100);
        if (result < 0 && errno != EINTR)
        {
            return;
        }
        if (result <= 0)
        {
            continue;
        }
        const ssize_t count = recv(fd, buffer + filled, sizeof(buffer) - filled, 0);
        if (count <= 0)
        {
            return;
        }
        filled += static_cast<size_t>(count);
        size_t used = 0;
        for (; filled - used >= sizeof(session::Message); used += sizeof(session::Message))
        {
            session::Message message;
            std::memcpy(&message, buffer + used, sizeof(message));
            Apply(message);
        }
        std::memmove(buffer, buffer + used, filled - used);
        filled -= used;
    }
}

void Run()
{
    while (g_running)
    {
        const int fd = Connect();
        if (fd < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }
        Receive(fd);
        close(fd);
        g_captured = false;
        g_state.Release();
    }
}
}

void input::Start()
{
    g_running = true;
    g_thread = std::thread(Run);
}

void input::Stop()
{
    if (!g_thread.joinable())
    {
        return;
    }
    g_running = false;
    g_thread.join();
}

InputFrame input::Poll()
{
    return g_state.Poll(g_captured);
}
