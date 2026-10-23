# Desktop VR

Play VRChat in **VR mode on a normal desktop**, with keyboard and mouse. No headset needed.

Desktop VR is a small SteamVR driver that pretends to be a headset and two Valve Index controllers, driven by your mouse and keyboard. VRChat sees a normal VR player, so you get VR-only features like grabbing things, real hand movement and finger gestures, while sitting at your desk.

It never touches VRChat or Easy Anti-Cheat. It works the same way as other SteamVR drivers such as SlimeVR.

## Requirements

- Windows 10 or 11 (64-bit)
- Steam, with **SteamVR** installed and started at least once
- **VRChat** from Steam
- A PC that runs VRChat well: VR mode draws the world twice (once per eye), so expect lower FPS than desktop mode

No headset, admin rights or extra installs are needed.

## Install

1. Download the latest zip from [Releases](../../releases).
2. Extract it anywhere you like (for example `Documents\Desktop VR`).
3. Run **Desktop VR.exe**.

On first start it adds its driver to SteamVR. After that it opens SteamVR and VRChat in VR mode, and closes SteamVR again when you quit VRChat. If VRChat or SteamVR is already open, it asks to restart them.

If you move the folder, just run Desktop VR.exe again from the new place.

The driver is only active while Desktop VR.exe is running. When you start SteamVR normally, your real headset works as usual. Turn off or unplug a real headset before using Desktop VR, or SteamVR may pick the headset instead.

## Controls

Controls only work while the VRChat window is focused.

| Key | Action |
|---|---|
| Mouse | Look around |
| Hold **Q** / **E** + mouse | Move left / right hand |
| Mouse wheel | Move the right hand closer / further. Left hand while Q is held |
| **R** | Reset hands |
| Left click | Trigger (use, click menus). Left hand while Q is held |
| Right click | Grip (grab). Left hand while Q is held |
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
| **F9** | Pause / resume capture (frees the mouse) |

Alt+Tab always works.

Your hands rest by your sides and rise when you use them, then lower again after a few seconds.

## Joining worlds and changing avatars

In Desktop VR, join worlds and switch avatars from outside the game: on the [VRChat website](https://vrchat.com/home) or with [VRCX](https://github.com/vrcx-team/VRCX). Same with just opening general menus, open it via third party, since I haven't been arsed to make it possible.

## Settings

Edit `driver\desktopvr\resources\settings\default.vrsettings` while SteamVR is closed:

| Setting | Default | What it does |
|---|---|---|
| `render_width` / `render_height` | 1920 / 1080 | Resolution VRChat renders each eye at. Lower it for more FPS |
| `fov_vertical` | 70 | Vertical field of view in degrees |
| `mouse_sensitivity` | 0.1 | Look speed |
| `hand_speed` | 0.0008 | Hand movement speed |
| `standing_height` | 1.65 | Eye height in metres |
| `crouch_height` / `prone_height` | 1.1 / 0.35 | Eye height when crouching / prone |
| `aim_pitch` | 40 | Angle of the hand pointer in degrees |
| `window_width` / `window_height` | 480 / 270 | Size of SteamVR's small headset window |

## Uninstall

Run **Uninstall.cmd** (or `Desktop VR.exe --uninstall`), then delete the folder.

## Good to know

- Windows SmartScreen or your antivirus may warn about Desktop VR.exe because it is not code-signed, and because the driver reads the keyboard and mouse (only while VRChat is focused). You can check and build everything yourself from this source.
- Use at your own risk. Desktop VR does not modify VRChat, but VRChat's rules are VRChat's call.
- Only VRChat on Steam is supported.

## Build from source

1. Get MSVC with [portable-msvc.py](https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977) and place its output folder at `tools\msvc` (so `tools\msvc\setup_x64.bat` exists).
2. Close SteamVR, then run `build.ps1` in PowerShell.

This builds `Desktop VR.exe` and `driver\desktopvr\bin\win64\driver_desktopvr.dll`.

## License

MIT. Includes the OpenVR driver header and code adapted from Valve's OpenVR hand skeleton sample, both BSD-3-Clause (see `third_party\openvr\LICENSE`, or `LICENSE-OpenVR.txt` in the download).
