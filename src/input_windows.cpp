#include "input.h"
#include "input_state.h"

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
InputState g_state;

std::array<bool, 256> g_keys{};
std::array<bool, 256> g_blocked{};
bool g_mouseLeft = false;
bool g_mouseRight = false;
bool g_paused = false;

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
    g_captured = captured;
    return captured;
}

Key ToKey(DWORD vk)
{
    switch (vk)
    {
    case 'W': return Key::W;
    case 'A': return Key::A;
    case 'S': return Key::S;
    case 'D': return Key::D;
    case 'Q': return Key::Q;
    case 'E': return Key::E;
    case 'R': return Key::R;
    case 'C': return Key::C;
    case 'Z': return Key::Z;
    case 'V': return Key::V;
    case '1': return Key::One;
    case '3': return Key::Three;
    case VK_SPACE: return Key::Space;
    case VK_ESCAPE: return Key::Escape;
    case VK_LEFT: return Key::Left;
    case VK_RIGHT: return Key::Right;
    case VK_LSHIFT: return Key::LeftShift;
    case VK_RSHIFT: return Key::RightShift;
    default:
        if (vk >= VK_F1 && vk <= VK_F8)
        {
            return static_cast<Key>(static_cast<int>(Key::F1) + static_cast<int>(vk - VK_F1));
        }
        return Key::Count;
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
        const Key key = ToKey(vk);
        std::lock_guard<std::mutex> lock(g_lock);
        const bool wasDown = g_keys[vk];
        g_keys[vk] = down;
        if (!down)
        {
            block = g_blocked[vk];
            if (key != Key::Count)
            {
                g_state.SetKey(key, false);
            }
        }
        else if (vk == VK_F9 && ForegroundEligible())
        {
            if (!wasDown)
            {
                g_paused = !g_paused;
            }
            block = true;
        }
        else if (key == Key::LeftShift || key == Key::RightShift)
        {
            g_state.SetKey(key, true);
        }
        else if (key != Key::Count && !g_keys[VK_LMENU] && !g_keys[VK_RMENU] && UpdateCapture())
        {
            g_state.SetKey(key, true);
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
                    g_state.AddMotion(info->pt.x - cursor.x, info->pt.y - cursor.y);
                }
                block = true;
            }
            break;
        case WM_LBUTTONDOWN:
            g_mouseLeft = captured;
            g_state.SetButton(MouseButton::Left, captured);
            block = captured;
            break;
        case WM_LBUTTONUP:
            block = g_mouseLeft;
            g_mouseLeft = false;
            g_state.SetButton(MouseButton::Left, false);
            break;
        case WM_RBUTTONDOWN:
            g_mouseRight = captured;
            g_state.SetButton(MouseButton::Right, captured);
            block = captured;
            break;
        case WM_RBUTTONUP:
            block = g_mouseRight;
            g_mouseRight = false;
            g_state.SetButton(MouseButton::Right, false);
            break;
        case WM_MOUSEWHEEL:
            if (captured)
            {
                g_state.AddWheel(static_cast<short>(HIWORD(info->mouseData)) / static_cast<double>(WHEEL_DELTA));
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
    return g_state.Poll(UpdateCapture());
}
