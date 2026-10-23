$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$obj = Join-Path $root 'build'
$msvc = Join-Path $root 'tools\msvc\setup_x64.bat'
if (-not (Test-Path $msvc)) { throw 'MSVC not found: put a portable MSVC in tools\msvc (see README)' }
New-Item -ItemType Directory -Force $obj, (Join-Path $root 'driver\desktopvr\bin\win64') | Out-Null

$flags = '/nologo /std:c++17 /O2 /EHsc /MT /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE'
$script = Join-Path $obj 'build.bat'
@"
@echo off
call "$msvc" || exit /b 1
cl $flags /LD /external:W0 /external:I "$root\third_party\openvr" /Fo"$obj/" "$root\src\*.cpp" /Fe"$root\driver\desktopvr\bin\win64\driver_desktopvr.dll" /link user32.lib || exit /b 1
rc /nologo /fo "$obj\launcher.res" "$root\launcher\launcher.rc" || exit /b 1
cl $flags /Fo"$obj/" "$root\launcher\launcher.cpp" "$obj\launcher.res" /Fe"$root\Desktop VR.exe" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib advapi32.lib || exit /b 1
"@ | Set-Content -Encoding ascii $script

cmd /c $script
$failed = $LASTEXITCODE -ne 0
Remove-Item -Recurse -Force $obj
Get-ChildItem "$root\driver\desktopvr\bin\win64\*.exp", "$root\driver\desktopvr\bin\win64\*.lib" -ErrorAction SilentlyContinue | Remove-Item
if ($failed) { throw 'Build failed' }
