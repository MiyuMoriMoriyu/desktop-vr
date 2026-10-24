#!/bin/sh
set -eu

root=$(cd "$(dirname "$0")" && pwd)
build="$root/build"
sdl_version=3.4.18
sdl_hash=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
sdl="$build/sdl-$sdl_version"
cxx=${CXX:-g++}
flags="-std=c++17 -O2 -Wall -Wextra -fvisibility=hidden"

mkdir -p "$build" "$root/driver/desktopvr/bin/linux64"

if [ ! -f "$sdl/lib/libSDL3.a" ]; then
    archive="$build/SDL3-$sdl_version.tar.gz"
    [ -f "$archive" ] || curl -fsSL -o "$archive" "https://github.com/libsdl-org/SDL/releases/download/release-$sdl_version/SDL3-$sdl_version.tar.gz"
    echo "$sdl_hash  $archive" | sha256sum -c --quiet
    tar -xzf "$archive" -C "$build"
    cmake -S "$build/SDL3-$sdl_version" -B "$build/sdl-build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$sdl" \
        -DCMAKE_INSTALL_LIBDIR=lib -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_AUDIO=OFF -DSDL_CAMERA=OFF \
        -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF >/dev/null
    cmake --build "$build/sdl-build" --parallel >/dev/null
    cmake --install "$build/sdl-build" >/dev/null
fi

driver_sources=$(ls "$root"/src/*.cpp | grep -v '_windows\.cpp$')
echo '{ global: HmdDriverFactory; local: *; };' > "$build/exports.map"
$cxx $flags -fPIC -shared -isystem "$root/third_party/openvr" $driver_sources -o "$root/driver/desktopvr/bin/linux64/driver_desktopvr.so" \
    -static-libstdc++ -static-libgcc -pthread -Wl,--version-script="$build/exports.map" -Wl,-z,defs -Wl,--as-needed

$cxx $flags -I"$root/src" -isystem "$sdl/include" "$root/launcher/launcher_linux.cpp" -o "$root/desktop-vr" \
    $(PKG_CONFIG_PATH="$sdl/lib/pkgconfig" pkg-config --static --libs sdl3) -static-libstdc++ -static-libgcc -Wl,--as-needed
