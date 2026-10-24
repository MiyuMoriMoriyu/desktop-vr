# Desktop VR

Play VRChat in **VR mode on a normal desktop**, with keyboard and mouse. No headset needed. Works on **Windows** and **Linux**.

Desktop VR is a small SteamVR driver that pretends to be a headset and two Valve Index controllers, driven by your mouse and keyboard. VRChat sees a normal VR player, so you get VR-only features like grabbing things, real hand movement and finger gestures, while sitting at your desk.

It never touches VRChat or Easy Anti-Cheat. It works the same way as other SteamVR drivers such as SlimeVR.

## Requirements

- **Windows** 10 or 11 (64-bit), or **Linux** (64-bit, glibc 2.29 or newer: Ubuntu 20.04, Debian 11, Fedora 31 or anything newer)
- Steam, with **SteamVR** installed and started at least once
- **VRChat** from Steam (on Linux: through Proton)
- A PC that runs VRChat well: VR mode draws the world twice (once per eye), so expect lower FPS than desktop mode

No headset, admin rights or extra installs are needed.

## Install on Windows

1. Download `DesktopVR-vX.Y.Z-Windows.zip` from [Releases](../../releases).
2. Extract it anywhere you like (for example `Documents\Desktop VR`).
3. Run **Desktop VR.exe**.

## Install on Linux

1. Download `DesktopVR-vX.Y.Z-Linux.tar.gz` from [Releases](../../releases).
2. Extract it somewhere in your home folder: `tar -xzf DesktopVR-*-Linux.tar.gz -C ~`
3. Run `~/desktop-vr/desktop-vr`.

Keep the folder inside your home folder: SteamVR runs its drivers in a container that can't see most other places.

## Starting

On first start Desktop VR adds its driver to SteamVR. After that it opens SteamVR and VRChat in VR mode, and closes SteamVR again when you quit VRChat. If VRChat or SteamVR is already open, it asks to restart them.

If you move the folder, just start Desktop VR again from the new place.

The driver is only active while Desktop VR is running. When you start SteamVR normally, your real headset works as usual. Turn off or unplug a real headset before using Desktop VR, or SteamVR may pick the headset instead.

If VRChat opens on your desktop instead of in VR, close it and start it from Steam with **Launch in VR mode** while Desktop VR is running.

## Controls

**Windows:** controls work while the VRChat window is focused. Alt+Tab always works.

**Linux:** Desktop VR opens a small window. Click it to take control of VRChat, and keep watching the VRChat window. Press **F9** or Alt+Tab to let go of the mouse again.

| Key | Action |
|---|---|
| Mouse | Look around |
| Hold **Q** / **E** + mouse | Move left / right hand |
| Hold **1** / **3** + mouse | Rotate left / right hand |
| Hold **1** / **3** + mouse wheel | Twist left / right hand |
| Mouse wheel | Move the right hand closer / further. Left hand while Q is held |
| **R** | Reset hands (position and rotation) |
| Left click | Trigger (use, click menus). Left hand while Q or 1 is held |
| Right click | Grip (grab). Left hand while Q or 1 is held |
| **W A S D** | Move |
| **Left / Right arrows** | Turn |
| **Space** | Jump |
| **V** | Mute / unmute mic |
| **Esc** | Menu |
| **C** | Crouch (toggle) |
| **Z** | Prone (toggle) |
| **F1 - F8** | Hand gestures, both hands |
| **Left Shift + F1 - F8** | Gesture on the left hand only |
| **Right Shift + F1 - F8** | Gesture on the right hand only |
| **F9** | Pause / resume control (frees the mouse) |

Your hands rest by your sides and rise when you use them, then lower again after a few seconds. A rotated hand keeps its rotation until you press **R**. You can hold a grab while rotating, to turn things you are holding.

## Joining worlds and changing avatars

In Desktop VR, join worlds and switch avatars from outside the game: on the [VRChat website](https://vrchat.com/home) or with [VRCX](https://github.com/vrcx-team/VRCX). Same with just opening general menus, open it via third party, since I haven't been arsed to make it possible.

## Settings

Edit `driver/desktopvr/resources/settings/default.vrsettings` while SteamVR is closed:

| Setting | Default | What it does |
|---|---|---|
| `render_width` / `render_height` | 1920 / 1080 | Resolution VRChat renders each eye at. Lower it for more FPS |
| `fov_vertical` | 70 | Vertical field of view in degrees |
| `mouse_sensitivity` | 0.1 | Look speed |
| `hand_speed` | 0.0008 | Hand movement speed |
| `rotate_speed` | 0.25 | Hand rotation speed |
| `standing_height` | 1.65 | Eye height in metres |
| `crouch_height` / `prone_height` | 1.1 / 0.35 | Eye height when crouching / prone |
| `aim_pitch` | 40 | Angle of the hand pointer in degrees |
| `window_width` / `window_height` | 480 / 270 | Size of SteamVR's small headset window |

## Uninstall

- **Windows:** run **Uninstall.cmd** (or `Desktop VR.exe --uninstall`), then delete the folder.
- **Linux:** run `./uninstall.sh` (or `./desktop-vr --uninstall`), then delete the folder.

## Good to know

- On Windows, SmartScreen or your antivirus may warn about Desktop VR.exe because it is not code-signed, and because the driver reads the keyboard and mouse (only while VRChat is focused). You can check and build everything yourself from this source.
- On Linux, Desktop VR never reads your keyboard or mouse system-wide: it only gets input while its own window is focused, and needs no special permissions.
- Use at your own risk. Desktop VR does not modify VRChat, but VRChat's rules are VRChat's call.
- Only VRChat on Steam is supported.
- Linux support is new. If something doesn't work, open an issue and attach `~/.local/share/Steam/logs/vrserver.txt`.

## Build from source

**Windows**

1. Get MSVC with [portable-msvc.py](https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977) and place its output folder at `tools\msvc` (so `tools\msvc\setup_x64.bat` exists).
2. Close SteamVR, then run `build.ps1` in PowerShell.

This builds `Desktop VR.exe` and `driver\desktopvr\bin\win64\driver_desktopvr.dll`.

**Linux**

Run `./build.sh` with g++, CMake, curl and the usual SDL build dependencies installed. It downloads and checks SDL 3, then builds `desktop-vr` and `driver/desktopvr/bin/linux64/driver_desktopvr.so`. Release builds use Valve's Steam Runtime SDK, so they run on older distros too:

```
podman run --rm -v "$PWD":/src -w /src registry.gitlab.steamos.cloud/steamrt/sniper/sdk ./build.sh
```

## License

MIT. Includes the OpenVR driver header and code adapted from Valve's OpenVR hand skeleton sample, both BSD-3-Clause (see `third_party/openvr/LICENSE`, or `LICENSE-OpenVR.txt` in the download). The Linux version includes SDL 3, zlib license (`LICENSE-SDL.txt` in the download).
