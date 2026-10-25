#include <SDL3/SDL.h>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "input_state.h"
#include "session_linux.h"

extern char** environ;

namespace
{
const char kTitle[] = "Desktop VR";
const char kDriverName[] = "desktopvr";

using Clock = std::chrono::steady_clock;

std::vector<pid_t> FindProcesses(const char* name)
{
    std::vector<pid_t> pids;
    DIR* proc = opendir("/proc");
    if (!proc)
    {
        return pids;
    }
    while (const dirent* entry = readdir(proc))
    {
        char* end = nullptr;
        const long pid = std::strtol(entry->d_name, &end, 10);
        if (pid <= 0 || *end != '\0')
        {
            continue;
        }
        std::ifstream file(std::string("/proc/") + entry->d_name + "/comm");
        std::string comm;
        if (std::getline(file, comm) && comm == name)
        {
            pids.push_back(static_cast<pid_t>(pid));
        }
    }
    closedir(proc);
    return pids;
}

bool IsRunning(const char* name)
{
    return !FindProcesses(name).empty();
}

bool WaitGone(const char* name, int timeoutMs)
{
    const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (IsRunning(name))
    {
        if (Clock::now() >= end)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return true;
}

void Warn(SDL_Window* window, const char* message)
{
    std::fprintf(stderr, "%s: %s\n", kTitle, message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, kTitle, message, window);
}

bool Ask(const char* message)
{
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
    };
    const SDL_MessageBoxData data{ SDL_MESSAGEBOX_INFORMATION, nullptr, kTitle, message, 2, buttons, nullptr };
    int choice = 0;
    return SDL_ShowMessageBox(&data, &choice) && choice == 1;
}

bool Exists(const std::string& path)
{
    return access(path.c_str(), F_OK) == 0;
}

std::string ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string Home()
{
    const char* home = std::getenv("HOME");
    return home ? home : "";
}

std::string ConfigFolder()
{
    const char* config = std::getenv("XDG_CONFIG_HOME");
    return config && config[0] == '/' ? config : Home() + "/.config";
}

std::string ReadOpenVRPaths()
{
    return ReadFile(ConfigFolder() + "/openvr/openvrpaths.vrpath");
}

std::string ExeFolder()
{
    char path[4096] = {};
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    const std::string exe(path, length > 0 ? static_cast<size_t>(length) : 0);
    return exe.substr(0, exe.find_last_of('/'));
}

std::string FirstRuntime(const std::string& paths)
{
    const size_t key = paths.find("\"runtime\"");
    const size_t list = key == std::string::npos ? key : paths.find('[', key);
    const size_t start = list == std::string::npos ? list : paths.find('"', list);
    if (start == std::string::npos)
    {
        return {};
    }
    std::string runtime;
    for (size_t i = start + 1; i < paths.size() && paths[i] != '"'; ++i)
    {
        if (paths[i] == '\\' && i + 1 < paths.size())
        {
            ++i;
        }
        runtime += paths[i];
    }
    return runtime;
}

std::string JsonEscaped(const std::string& text)
{
    std::string result;
    for (char c : text)
    {
        if (c == '\\' || c == '"')
        {
            result += '\\';
        }
        result += c;
    }
    return result;
}

std::string FindSteamVR()
{
    const char startup[] = "/bin/vrstartup.sh";
    const std::string home = Home();
    for (const std::string& runtime : { FirstRuntime(ReadOpenVRPaths()), home + "/.local/share/Steam/steamapps/common/SteamVR",
             home + "/.steam/steam/steamapps/common/SteamVR", home + "/.steam/root/steamapps/common/SteamVR" })
    {
        if (!runtime.empty() && Exists(runtime + startup))
        {
            return runtime;
        }
    }
    return {};
}

pid_t Spawn(const std::vector<std::string>& arguments)
{
    std::vector<char*> argv;
    for (const std::string& argument : arguments)
    {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t files;
    posix_spawn_file_actions_init(&files);
    for (int fd = 0; fd <= 2; ++fd)
    {
        posix_spawn_file_actions_addopen(&files, fd, "/dev/null", fd == 0 ? O_RDONLY : O_WRONLY, 0);
    }
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t signals;
    sigemptyset(&signals);
    posix_spawnattr_setsigmask(&attributes, &signals);
    sigfillset(&signals);
    posix_spawnattr_setsigdefault(&attributes, &signals);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

    pid_t pid = -1;
    if (posix_spawnp(&pid, argv[0], &files, &attributes, argv.data(), environ) != 0)
    {
        pid = -1;
    }
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&files);
    return pid;
}

bool RunAndWait(const std::vector<std::string>& arguments)
{
    const pid_t pid = Spawn(arguments);
    if (pid < 0)
    {
        return false;
    }
    const auto end = Clock::now() + std::chrono::seconds(30);
    int status = 0;
    while (waitpid(pid, &status, WNOHANG) == 0)
    {
        if (Clock::now() >= end)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool PathReg(const std::string& runtime, std::initializer_list<std::string> arguments)
{
    std::vector<std::string> command{ runtime + "/bin/vrpathreg.sh" };
    command.insert(command.end(), arguments);
    return RunAndWait(command);
}

bool RegisterDriver(const std::string& runtime)
{
    const std::string driver = ExeFolder() + "/driver/" + kDriverName;
    if (!Exists(driver + "/driver.vrdrivermanifest"))
    {
        return false;
    }
    if (ReadOpenVRPaths().find("\"" + JsonEscaped(driver) + "\"") != std::string::npos)
    {
        return true;
    }
    PathReg(runtime, { "removedriverswithname", kDriverName });
    return PathReg(runtime, { "adddriver", driver });
}

void Uninstall()
{
    const std::string runtime = FindSteamVR();
    if (runtime.empty())
    {
        Warn(nullptr, "SteamVR was not found, so there is nothing to remove.");
        return;
    }
    if (!PathReg(runtime, { "removedriverswithname", kDriverName }))
    {
        Warn(nullptr, "Desktop VR could not be removed from SteamVR. Close SteamVR, then try again.");
        return;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, kTitle, "Desktop VR was removed from SteamVR. You can delete this folder now.", nullptr);
}

void Signal(std::initializer_list<const char*> names, int signal)
{
    for (const char* name : names)
    {
        for (pid_t pid : FindProcesses(name))
        {
            kill(pid, signal);
        }
    }
}

void CloseSteamVR()
{
    Signal({ "vrmonitor", "vrserver" }, SIGTERM);
    WaitGone("vrserver", 15000);
    Signal({ "vrmonitor", "vrcompositor", "vrwebhelper", "vrdashboard", "vrserver" }, SIGKILL);
}

void CloseVRChat()
{
    Signal({ "VRChat.exe" }, SIGTERM);
    WaitGone("VRChat.exe", 20000);
    Signal({ "VRChat.exe" }, SIGKILL);
}

bool OpenSteam(const char* url)
{
    return Spawn({ "steam", url }) > 0 || Spawn({ "xdg-open", url }) > 0;
}

int Listen()
{
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0)
    {
        return -1;
    }
    sockaddr_un address;
    const socklen_t length = session::Address(&address);
    if (bind(fd, reinterpret_cast<const sockaddr*>(&address), length) != 0 || listen(fd, 4) != 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

Key ToKey(SDL_Scancode scancode)
{
    switch (scancode)
    {
    case SDL_SCANCODE_W: return Key::W;
    case SDL_SCANCODE_A: return Key::A;
    case SDL_SCANCODE_S: return Key::S;
    case SDL_SCANCODE_D: return Key::D;
    case SDL_SCANCODE_Q: return Key::Q;
    case SDL_SCANCODE_E: return Key::E;
    case SDL_SCANCODE_R: return Key::R;
    case SDL_SCANCODE_C: return Key::C;
    case SDL_SCANCODE_Z: return Key::Z;
    case SDL_SCANCODE_V: return Key::V;
    case SDL_SCANCODE_1: return Key::One;
    case SDL_SCANCODE_3: return Key::Three;
    case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_ESCAPE: return Key::Escape;
    case SDL_SCANCODE_LEFT: return Key::Left;
    case SDL_SCANCODE_RIGHT: return Key::Right;
    case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT: return Key::RightShift;
    default:
        if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F8)
        {
            return static_cast<Key>(static_cast<int>(Key::F1) + (scancode - SDL_SCANCODE_F1));
        }
        return Key::Count;
    }
}

MouseButton ToButton(Uint8 button)
{
    switch (button)
    {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    default: return MouseButton::Count;
    }
}

class Session
{
public:
    explicit Session(int server)
        : server_(server)
    {
    }

    ~Session()
    {
        Disconnect();
        close(server_);
    }

    void Update(SDL_Window* window, bool captured)
    {
        for (int fd; (fd = accept4(server_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK)) >= 0;)
        {
            if (!session::SameUser(fd))
            {
                close(fd);
                continue;
            }
            Disconnect();
            client_ = fd;
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
            Send({ session::MessageType::Hello, 0, 0, 0, mode ? mode->refresh_rate : 0.f, 0.f });
            Send({ session::MessageType::Capture, 0, captured, 0, 0.f, 0.f });
        }
        char probe;
        if (client_ >= 0 && recv(client_, &probe, 1, MSG_DONTWAIT | MSG_PEEK) == 0)
        {
            Disconnect();
        }
    }

    void Send(const session::Message& message)
    {
        if (client_ >= 0 && send(client_, &message, sizeof(message), MSG_NOSIGNAL | MSG_DONTWAIT) != sizeof(message))
        {
            Disconnect();
        }
    }

private:
    void Disconnect()
    {
        if (client_ >= 0)
        {
            close(client_);
            client_ = -1;
        }
    }

    int server_;
    int client_ = -1;
};

class Capture
{
public:
    Capture(SDL_Window* window, Session& session)
        : window_(window)
        , session_(session)
    {
    }

    bool Active() const { return active_; }

    void Start()
    {
        if (active_)
        {
            return;
        }
        SDL_SetWindowRelativeMouseMode(window_, true);
        active_ = true;
        session_.Send({ session::MessageType::Capture, 0, 1, 0, 0.f, 0.f });
    }

    void Stop()
    {
        if (!active_)
        {
            return;
        }
        for (size_t key = 0; key < keys_.size(); ++key)
        {
            SetKey(static_cast<Key>(key), false);
        }
        for (size_t button = 0; button < buttons_.size(); ++button)
        {
            SetButton(static_cast<MouseButton>(button), false);
        }
        SDL_SetWindowRelativeMouseMode(window_, false);
        active_ = false;
        session_.Send({ session::MessageType::Capture, 0, 0, 0, 0.f, 0.f });
    }

    void Handle(const SDL_Event& event)
    {
        switch (event.type)
        {
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            Stop();
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.repeat)
            {
                break;
            }
            if (event.key.scancode == SDL_SCANCODE_F9)
            {
                active_ ? Stop() : Start();
            }
            else if (active_ && ToKey(event.key.scancode) != Key::Count)
            {
                SetKey(ToKey(event.key.scancode), true);
            }
            break;
        case SDL_EVENT_KEY_UP:
            if (ToKey(event.key.scancode) != Key::Count)
            {
                SetKey(ToKey(event.key.scancode), false);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!active_)
            {
                Start();
                ignoredButton_ = event.button.button;
            }
            else if (ToButton(event.button.button) != MouseButton::Count)
            {
                SetButton(ToButton(event.button.button), true);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == ignoredButton_)
            {
                ignoredButton_ = 0;
            }
            else if (ToButton(event.button.button) != MouseButton::Count)
            {
                SetButton(ToButton(event.button.button), false);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (active_)
            {
                session_.Send({ session::MessageType::Motion, 0, 0, 0, event.motion.xrel, event.motion.yrel });
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (active_)
            {
                const float steps = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
                session_.Send({ session::MessageType::Wheel, 0, 0, 0, steps, 0.f });
            }
            break;
        default:
            break;
        }
    }

private:
    void SetKey(Key key, bool down)
    {
        bool& held = keys_[static_cast<size_t>(key)];
        if (held != down)
        {
            held = down;
            session_.Send({ session::MessageType::Key, static_cast<uint8_t>(key), down, 0, 0.f, 0.f });
        }
    }

    void SetButton(MouseButton button, bool down)
    {
        bool& held = buttons_[static_cast<size_t>(button)];
        if (held != down)
        {
            held = down;
            session_.Send({ session::MessageType::Button, static_cast<uint8_t>(button), down, 0, 0.f, 0.f });
        }
    }

    SDL_Window* window_;
    Session& session_;
    bool active_ = false;
    Uint8 ignoredButton_ = 0;
    std::array<bool, static_cast<size_t>(Key::Count)> keys_{};
    std::array<bool, static_cast<size_t>(MouseButton::Count)> buttons_{};
};

enum class Stage
{
    StartingSteamVR,
    StartingGame,
    Playing
};

const char* WindowTitle(Stage stage, bool captured)
{
    if (captured)
    {
        return "Desktop VR - controlling VRChat (F9 to stop)";
    }
    switch (stage)
    {
    case Stage::StartingSteamVR: return "Desktop VR - starting SteamVR";
    case Stage::StartingGame: return "Desktop VR - starting VRChat";
    default: return "Desktop VR - click here to control VRChat";
    }
}

void FillCircle(SDL_Renderer* renderer, float x, float y, float radius, SDL_FColor color)
{
    constexpr int kSegments = 96;
    std::array<SDL_Vertex, kSegments + 1> vertices{};
    std::array<int, kSegments * 3> indices{};
    vertices[0] = { { x, y }, color, { 0.f, 0.f } };
    for (int i = 0; i < kSegments; ++i)
    {
        const float angle = 2.f * static_cast<float>(M_PI) * static_cast<float>(i) / kSegments;
        vertices[i + 1] = { { x + std::cos(angle) * radius, y + std::sin(angle) * radius }, color, { 0.f, 0.f } };
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % kSegments + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
}

SDL_FColor Color(Uint32 rgb)
{
    return { ((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, 1.f };
}

void Draw(SDL_Renderer* renderer, Stage stage, bool captured)
{
    const SDL_FColor background = Color(0x0E0915);
    SDL_SetRenderDrawColorFloat(renderer, background.r, background.g, background.b, 1.f);
    SDL_RenderClear(renderer);
    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
    const float x = width * 0.5f;
    const float y = height * 0.5f;
    const float radius = std::min(width, height) * 0.16f;
    if (captured)
    {
        FillCircle(renderer, x, y, radius, Color(0xB48CFF));
    }
    else
    {
        FillCircle(renderer, x, y, radius, Color(stage == Stage::Playing ? 0x9486B8 : 0x352A4D));
        FillCircle(renderer, x, y, radius * 0.86f, background);
    }
    SDL_RenderPresent(renderer);
}

int Play(int server)
{
    if (IsRunning("VRChat.exe") || IsRunning("vrserver"))
    {
        if (!Ask("VRChat or SteamVR is already open. Restart them in Desktop VR?"))
        {
            return 0;
        }
        CloseVRChat();
        CloseSteamVR();
        if (IsRunning("VRChat.exe") || !WaitGone("vrserver", 5000))
        {
            Warn(nullptr, "VRChat or SteamVR could not be closed. Close them, then start Desktop VR again.");
            return 0;
        }
    }
    const std::string runtime = FindSteamVR();
    if (runtime.empty())
    {
        Warn(nullptr, "SteamVR was not found. Install SteamVR from Steam, then start Desktop VR again.");
        return 0;
    }
    if (!RegisterDriver(runtime))
    {
        Warn(nullptr, "The Desktop VR driver could not be added to SteamVR. Keep the driver folder next to desktop-vr.");
        return 0;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(WindowTitle(Stage::StartingSteamVR, false), 420, 180, SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer))
    {
        Warn(nullptr, SDL_GetError());
        return 1;
    }

    Session session(server);
    Capture capture(window, session);
    Stage stage = Stage::StartingSteamVR;
    bool gameStarted = false;
    bool running = OpenSteam("steam://rungameid/250820");
    if (!running)
    {
        Warn(window, "SteamVR could not be started.");
    }
    auto deadline = Clock::now() + std::chrono::seconds(120);
    auto nextCheck = Clock::now();
    auto steamVRSeen = Clock::now();
    bool dirty = true;
    bool shownCapture = false;

    while (running)
    {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 50))
        {
            do
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                }
                else if (event.type == SDL_EVENT_WINDOW_EXPOSED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                {
                    dirty = true;
                }
                capture.Handle(event);
            } while (SDL_PollEvent(&event));
        }
        session.Update(window, capture.Active());
        while (waitpid(-1, nullptr, WNOHANG) > 0)
        {
        }

        const auto now = Clock::now();
        if (now >= nextCheck)
        {
            nextCheck = now + std::chrono::seconds(1);
            const bool steamVR = IsRunning("vrserver");
            const bool game = IsRunning("VRChat.exe");
            if (steamVR)
            {
                steamVRSeen = now;
            }
            if (stage == Stage::StartingSteamVR)
            {
                if (steamVR)
                {
                    OpenSteam("steam://launch/438100/VR");
                    stage = Stage::StartingGame;
                    deadline = now + std::chrono::seconds(180);
                    dirty = true;
                }
                else if (now >= deadline)
                {
                    Warn(window, "SteamVR did not start.");
                    running = false;
                }
            }
            else if (!steamVR)
            {
                running = now - steamVRSeen < std::chrono::seconds(15);
            }
            else if (game && !gameStarted)
            {
                gameStarted = true;
                stage = Stage::Playing;
                dirty = true;
            }
            else if (gameStarted && !game)
            {
                capture.Stop();
                CloseSteamVR();
                running = false;
            }
            else if (stage == Stage::StartingGame && now >= deadline)
            {
                stage = Stage::Playing;
                dirty = true;
            }
        }

        if (capture.Active() != shownCapture)
        {
            shownCapture = capture.Active();
            dirty = true;
        }
        if (dirty && running)
        {
            dirty = false;
            SDL_SetWindowTitle(window, WindowTitle(stage, shownCapture));
            Draw(renderer, stage, shownCapture);
        }
    }

    capture.Stop();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return 0;
}
}

int main(int argc, char** argv)
{
    SDL_SetAppMetadata(kTitle, nullptr, "desktop-vr");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::fprintf(stderr, "%s: %s\n", kTitle, SDL_GetError());
        return 1;
    }
    int result = 0;
    if (argc > 1 && std::strcmp(argv[1], "--uninstall") == 0)
    {
        Uninstall();
    }
    else
    {
        const int server = Listen();
        if (server >= 0)
        {
            result = Play(server);
        }
    }
    SDL_Quit();
    return result;
}
