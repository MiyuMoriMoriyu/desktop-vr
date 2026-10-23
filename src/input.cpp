#include "input.h"

#include <windows.h>
#include <tlhelp32.h>

#include <mutex>
#include <thread>
#include <utility>

namespace
{
std::mutex g_lock;
std::thread g_thread;
DWORD g_threadId = 0;

std::array<bool, 256> g_keys{};
std::array<bool, 256> g_blocked{};
bool g_mouseLeft = false;
bool g_mouseRight = false;
double g_dx = 0.0;
double g_dy = 0.0;
double g_wheel = 0.0;
bool g_paused = false;
bool g_crouch = false;
bool g_prone = false;
bool g_resetHands = false;
std::array<int, 2> g_gestures{};

HWND g_foreground = nullptr;
bool g_eligible = false;
bool g_captured = false;
bool g_recentered = false;

bool IsGameProcess(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool match = false;
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry))
    {
        if (entry.th32ProcessID == pid)
        {
            match = _wcsicmp(entry.szExeFile, L"VRChat.exe") == 0;
            break;
        }
    }
    CloseHandle(snapshot);
    return match;
}

bool ForegroundEligible()
{
    HWND window = GetForegroundWindow();
    if (window != g_foreground)
    {
        g_foreground = window;
        g_paused = false;
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        g_eligible = pid != 0 && IsGameProcess(pid);
    }
    return g_eligible;
}

bool UpdateCapture()
{
    const bool captured = ForegroundEligible() && !g_paused;
    if (captured && !g_captured)
    {
        RECT rect;
        if (GetWindowRect(g_foreground, &rect))
        {
            SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
            g_recentered = true;
        }
    }
    if (!captured)
    {
        g_dx = 0.0;
        g_dy = 0.0;
        g_wheel = 0.0;
    }
    g_captured = captured;
    return captured;
}

bool IsMappedKey(DWORD vk)
{
    switch (vk)
    {
    case 'W':
    case 'A':
    case 'S':
    case 'D':
    case 'Q':
    case 'E':
    case 'R':
    case 'C':
    case 'Z':
    case 'V':
    case VK_SPACE:
    case VK_ESCAPE:
    case VK_LEFT:
    case VK_RIGHT:
        return true;
    default:
        return vk >= VK_F1 && vk <= VK_F8;
    }
}

void OnPress(DWORD vk)
{
    if (vk >= VK_F1 && vk <= VK_F8)
    {
        const int gesture = static_cast<int>(vk - VK_F1);
        const bool leftShift = g_keys[VK_LSHIFT];
        const bool rightShift = g_keys[VK_RSHIFT];
        if (leftShift || !rightShift)
        {
            g_gestures[0] = gesture;
        }
        if (rightShift || !leftShift)
        {
            g_gestures[1] = gesture;
        }
    }
    else if (vk == 'C')
    {
        g_crouch = !g_crouch;
        g_prone = false;
    }
    else if (vk == 'Z')
    {
        g_prone = !g_prone;
        g_crouch = false;
    }
    else if (vk == 'R')
    {
        g_resetHands = true;
    }
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    bool block = false;
    const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (code == HC_ACTION && !(info->flags & LLKHF_INJECTED) && info->vkCode < 256)
    {
        const DWORD vk = info->vkCode;
        const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        std::lock_guard<std::mutex> lock(g_lock);
        const bool wasDown = g_keys[vk];
        g_keys[vk] = down;
        if (!down)
        {
            block = g_blocked[vk];
        }
        else if (vk == VK_F9 && ForegroundEligible())
        {
            if (!wasDown)
            {
                g_paused = !g_paused;
            }
            block = true;
        }
        else if (IsMappedKey(vk) && !g_keys[VK_LMENU] && !g_keys[VK_RMENU] && UpdateCapture())
        {
            if (!wasDown)
            {
                OnPress(vk);
            }
            block = true;
        }
        g_blocked[vk] = down && block;
    }
    return block ? 1 : CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    bool block = false;
    const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
    if (code == HC_ACTION && !(info->flags & LLMHF_INJECTED))
    {
        std::lock_guard<std::mutex> lock(g_lock);
        const bool captured = UpdateCapture();
        switch (wParam)
        {
        case WM_MOUSEMOVE:
            if (captured)
            {
                POINT cursor;
                if (!std::exchange(g_recentered, false) && GetCursorPos(&cursor))
                {
                    g_dx += info->pt.x - cursor.x;
                    g_dy += info->pt.y - cursor.y;
                }
                block = true;
            }
            break;
        case WM_LBUTTONDOWN:
            g_mouseLeft = captured;
            block = captured;
            break;
        case WM_LBUTTONUP:
            block = g_mouseLeft;
            g_mouseLeft = false;
            break;
        case WM_RBUTTONDOWN:
            g_mouseRight = captured;
            block = captured;
            break;
        case WM_RBUTTONUP:
            block = g_mouseRight;
            g_mouseRight = false;
            break;
        case WM_MOUSEWHEEL:
            if (captured)
            {
                g_wheel += static_cast<short>(HIWORD(info->mouseData)) / static_cast<double>(WHEEL_DELTA);
                block = true;
            }
            break;
        default:
            break;
        }
    }
    return block ? 1 : CallNextHookEx(nullptr, code, wParam, lParam);
}

void Run()
{
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&KeyboardProc), &module);
    HHOOK keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, module, 0);
    HHOOK mouse = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, module, 0);

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        DispatchMessageW(&message);
    }

    if (mouse)
    {
        UnhookWindowsHookEx(mouse);
    }
    if (keyboard)
    {
        UnhookWindowsHookEx(keyboard);
    }
}
}

void input::Start()
{
    g_thread = std::thread(Run);
    g_threadId = GetThreadId(g_thread.native_handle());
}

void input::Stop()
{
    if (!g_thread.joinable())
    {
        return;
    }
    while (!PostThreadMessageW(g_threadId, WM_QUIT, 0, 0))
    {
        Sleep(1);
    }
    g_thread.join();
}

InputFrame input::Poll()
{
    std::lock_guard<std::mutex> lock(g_lock);
    InputFrame frame;
    frame.captured = UpdateCapture();
    frame.crouch = g_crouch;
    frame.prone = g_prone;
    frame.resetHands = std::exchange(g_resetHands, false);
    frame.hands[0].gesture = g_gestures[0];
    frame.hands[1].gesture = g_gestures[1];
    if (!frame.captured)
    {
        return frame;
    }

    frame.lookX = std::exchange(g_dx, 0.0);
    frame.lookY = std::exchange(g_dy, 0.0);
    frame.wheel = std::exchange(g_wheel, 0.0);

    const bool left = g_keys['Q'];
    frame.movingHand = left ? 0 : (g_keys['E'] ? 1 : -1);
    HandInput& active = frame.hands[left ? 0 : 1];
    active.trigger = g_mouseLeft;
    active.grip = g_mouseRight;

    float moveX = static_cast<float>(g_keys['D']) - static_cast<float>(g_keys['A']);
    float moveY = static_cast<float>(g_keys['W']) - static_cast<float>(g_keys['S']);
    if (moveX != 0.f && moveY != 0.f)
    {
        moveX *= 0.7071f;
        moveY *= 0.7071f;
    }
    frame.hands[0].stickX = moveX;
    frame.hands[0].stickY = moveY;
    frame.hands[1].stickX = static_cast<float>(g_keys[VK_RIGHT]) - static_cast<float>(g_keys[VK_LEFT]);

    frame.hands[0].a = g_keys['V'];
    frame.hands[0].b = g_keys[VK_ESCAPE];
    frame.hands[1].a = g_keys[VK_SPACE];
    return frame;
}
