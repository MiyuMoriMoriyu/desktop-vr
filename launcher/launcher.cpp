#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

namespace
{
const wchar_t kTitle[] = L"Desktop VR";
const wchar_t kDriverName[] = L"desktopvr";

std::vector<DWORD> FindProcesses(const wchar_t* name)
{
    std::vector<DWORD> pids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return pids;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry))
    {
        if (_wcsicmp(entry.szExeFile, name) == 0)
        {
            pids.push_back(entry.th32ProcessID);
        }
    }
    CloseHandle(snapshot);
    return pids;
}

bool IsRunning(const wchar_t* name)
{
    return !FindProcesses(name).empty();
}

bool WaitFor(const wchar_t* name, DWORD timeoutMs)
{
    const ULONGLONG end = GetTickCount64() + timeoutMs;
    while (GetTickCount64() < end)
    {
        if (IsRunning(name))
        {
            return true;
        }
        Sleep(1000);
    }
    return false;
}

void Warn(const wchar_t* message)
{
    MessageBoxW(nullptr, message, kTitle, MB_OK | MB_ICONWARNING);
}

bool Exists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring Widen(const std::string& text)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::wstring ExeFolder()
{
    wchar_t path[MAX_PATH] = {};
    const std::wstring exe(path, GetModuleFileNameW(nullptr, path, MAX_PATH));
    return exe.substr(0, exe.find_last_of(L'\\'));
}

std::wstring ReadOpenVRPaths()
{
    wchar_t local[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH))
    {
        return {};
    }
    std::ifstream file(std::wstring(local) + L"\\openvr\\openvrpaths.vrpath", std::ios::binary);
    return Widen(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
}

std::wstring FirstRuntime(const std::wstring& paths)
{
    const size_t key = paths.find(L"\"runtime\"");
    const size_t list = key == std::wstring::npos ? key : paths.find(L'[', key);
    const size_t start = list == std::wstring::npos ? list : paths.find(L'"', list);
    if (start == std::wstring::npos)
    {
        return {};
    }
    std::wstring runtime;
    for (size_t i = start + 1; i < paths.size() && paths[i] != L'"'; ++i)
    {
        if (paths[i] == L'\\' && i + 1 < paths.size())
        {
            ++i;
        }
        runtime += paths[i];
    }
    return runtime;
}

std::wstring JsonEscaped(const std::wstring& text)
{
    std::wstring result;
    for (wchar_t c : text)
    {
        if (c == L'\\' || c == L'"')
        {
            result += L'\\';
        }
        result += c;
    }
    return result;
}

std::wstring FindSteamVR()
{
    const wchar_t startup[] = L"\\bin\\win64\\vrstartup.exe";
    const std::wstring runtime = FirstRuntime(ReadOpenVRPaths());
    if (!runtime.empty() && Exists(runtime + startup))
    {
        return runtime;
    }
    wchar_t steam[MAX_PATH] = {};
    DWORD size = sizeof(steam);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, steam, &size) != ERROR_SUCCESS)
    {
        return {};
    }
    const std::wstring fallback = std::wstring(steam) + L"\\steamapps\\common\\SteamVR";
    return Exists(fallback + startup) ? fallback : std::wstring();
}

bool PathReg(const std::wstring& runtime, const std::wstring& arguments)
{
    const std::wstring exe = runtime + L"\\bin\\win64\\vrpathreg.exe";
    std::wstring command = L"\"" + exe + L"\" " + arguments;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
    {
        return false;
    }
    DWORD code = 1;
    if (WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0)
    {
        GetExitCodeProcess(process.hProcess, &code);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0;
}

bool RegisterDriver(const std::wstring& runtime)
{
    const std::wstring driver = ExeFolder() + L"\\driver\\" + kDriverName;
    if (!Exists(driver + L"\\driver.vrdrivermanifest"))
    {
        return false;
    }
    if (ReadOpenVRPaths().find(L"\"" + JsonEscaped(driver) + L"\"") != std::wstring::npos)
    {
        return true;
    }
    PathReg(runtime, std::wstring(L"removedriverswithname ") + kDriverName);
    return PathReg(runtime, L"adddriver \"" + driver + L"\"");
}

void Uninstall()
{
    const std::wstring runtime = FindSteamVR();
    if (runtime.empty())
    {
        Warn(L"SteamVR was not found, so there is nothing to remove.");
        return;
    }
    if (!PathReg(runtime, std::wstring(L"removedriverswithname ") + kDriverName))
    {
        Warn(L"Desktop VR could not be removed from SteamVR. Close SteamVR, then try again.");
        return;
    }
    MessageBoxW(nullptr, L"Desktop VR was removed from SteamVR. You can delete this folder now.", kTitle, MB_OK | MB_ICONINFORMATION);
}

BOOL CALLBACK CloseWindowOf(HWND window, LPARAM pid)
{
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner == static_cast<DWORD>(pid) && IsWindowVisible(window))
    {
        PostMessageW(window, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

bool WaitGone(const wchar_t* name, DWORD timeoutMs)
{
    const ULONGLONG end = GetTickCount64() + timeoutMs;
    while (IsRunning(name))
    {
        if (GetTickCount64() >= end)
        {
            return false;
        }
        Sleep(500);
    }
    return true;
}

void Close(const wchar_t* windowOwner, const wchar_t* mainProcess, std::initializer_list<const wchar_t*> leftovers, DWORD timeoutMs)
{
    for (DWORD pid : FindProcesses(windowOwner))
    {
        EnumWindows(CloseWindowOf, static_cast<LPARAM>(pid));
    }
    WaitGone(mainProcess, timeoutMs);
    for (const wchar_t* name : leftovers)
    {
        for (DWORD pid : FindProcesses(name))
        {
            HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (process)
            {
                TerminateProcess(process, 0);
                CloseHandle(process);
            }
        }
    }
}

void CloseSteamVR()
{
    Close(L"vrmonitor.exe", L"vrserver.exe", { L"vrmonitor.exe", L"vrcompositor.exe", L"vrdashboard.exe", L"vrserver.exe" }, 15000);
}

void CloseVRChat()
{
    Close(L"VRChat.exe", L"VRChat.exe", { L"VRChat.exe" }, 20000);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int)
{
    if (_wcsicmp(arguments, L"--uninstall") == 0)
    {
        Uninstall();
        return 0;
    }
    HANDLE session = CreateMutexW(nullptr, TRUE, L"Local\\DesktopVRSession");
    if (!session || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return 0;
    }
    if (IsRunning(L"VRChat.exe") || IsRunning(L"vrserver.exe"))
    {
        if (MessageBoxW(nullptr, L"VRChat or SteamVR is already open. Restart them in Desktop VR?", kTitle, MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return 0;
        }
        CloseVRChat();
        CloseSteamVR();
        if (IsRunning(L"VRChat.exe") || !WaitGone(L"vrserver.exe", 5000))
        {
            Warn(L"VRChat or SteamVR could not be closed. Close them, then start Desktop VR again.");
            return 0;
        }
    }
    const std::wstring runtime = FindSteamVR();
    if (runtime.empty())
    {
        Warn(L"SteamVR was not found. Install SteamVR from Steam, then start Desktop VR again.");
        return 0;
    }
    if (!RegisterDriver(runtime))
    {
        Warn(L"The Desktop VR driver could not be added to SteamVR. Keep the driver folder next to Desktop VR.exe.");
        return 0;
    }

    ShellExecuteW(nullptr, L"open", (runtime + L"\\bin\\win64\\vrstartup.exe").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (!WaitFor(L"vrserver.exe", 60000))
    {
        Warn(L"SteamVR did not start.");
        return 0;
    }

    ShellExecuteW(nullptr, L"open", L"steam://launch/438100/VR", nullptr, nullptr, SW_SHOWNORMAL);
    const bool gameStarted = WaitFor(L"VRChat.exe", 180000);

    while (IsRunning(L"vrserver.exe"))
    {
        if (gameStarted && !IsRunning(L"VRChat.exe"))
        {
            CloseSteamVR();
            break;
        }
        Sleep(2000);
    }
    CloseHandle(session);
    return 0;
}
