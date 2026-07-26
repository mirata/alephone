# Aleph One — Meta Quest VR

> **⚠️ This is the `questvr` branch — an experimental Meta Quest / Horizon OS VR port of Aleph One.**
> It adds a stereoscopic, head-tracked, room-scale VR renderer and OpenXR input on top of upstream
> Aleph One, and builds as a native Android `.apk` for the Quest 2 / 3 / 3S / Pro. The desktop
> (Windows / macOS / Linux) builds still work unchanged — all VR code is `#if defined(__ANDROID__)`
> guarded. If you want the standard, non-VR game, use the [upstream repository](https://github.com/Aleph-One-Marathon/alephone).
>
> **Jump to:** [Build the Quest VR APK](#meta-quest-vr-build-this-branch)

Aleph One is the open source continuation of Bungie™’s _Marathon® 2_ and _Marathon Infinity_ game engines. Aleph One plays _Marathon_, _Marathon 2_, _Marathon Infinity_, and third-party content on a variety of platforms.

Aleph One is available under the terms of the [GNU General Public License (GPL 3)](http://www.gnu.org/licenses/gpl-3.0.html)

[![Discord](https://img.shields.io/badge/Discord-%235865F2.svg?&logo=discord&logoColor=white)](https://discord.gg/NvF3pdV)     [![Steam](https://img.shields.io/badge/steam-%23000000.svg?style=for-the-badge&logo=steam&logoColor=white)](https://store.steampowered.com/developer/alephone)

# Download

To download ready-to-run versions of all three _Marathon_ games for macOS,
Windows, and Linux Flatpak, visit
[alephone.lhowon.org](https://alephone.lhowon.org)

# Meta Quest VR build (this branch)

The VR build produces a native Android `.apk` for the Quest. It reuses the same engine and the same
[vcpkg](https://github.com/microsoft/vcpkg) dependencies as the desktop build, cross-compiled for
`arm64-v8a`, driven by Gradle + CMake. These instructions are for a **Windows** host (the port is
developed on Windows 11 with PowerShell), but the Gradle build works from any host with the Android
toolchain installed.

## Prerequisites

1. **Android Studio** — https://developer.android.com/studio. It bundles a JDK (used below as
   `JAVA_HOME`) and the SDK Manager. Using the SDK Manager, install:
   - SDK Platform **android-35**
   - **NDK (Side by side)** version **30.0.14904198** (must match `ndkVersion` in `android/app/build.gradle`)
   - **CMake** (AGP fetches its own bundled 3.22.1, so any recent version is fine)
   - **Android SDK Platform-Tools** (gives you `adb`) and **Build-Tools**
2. **vcpkg** — clone it (a short, space-free path is recommended, e.g. `C:\Projects\vcpkg`):
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\Projects\vcpkg
   C:\Projects\vcpkg\bootstrap-vcpkg.bat
   ```
3. **Clone this repo with submodules** (scenario data lives in submodules):
   ```powershell
   git clone --recurse-submodules -b questvr https://github.com/<your-fork>/alephone.git
   ```

## Build the Android dependencies (one-time)

The build reads the arm64-android dependencies from `android/jni/vcpkg_installed`, which is **not**
checked in, so build them once with vcpkg. The `arm64-android` triplet reads `ANDROID_NDK_HOME`:

```powershell
$env:ANDROID_NDK_HOME = "$env:LOCALAPPDATA\Android\Sdk\ndk\30.0.14904198"
& C:\Projects\vcpkg\vcpkg.exe install --triplet arm64-android --x-install-root=C:\Projects\alephone\android\jni\vcpkg_installed
```

This is a long first run (Boost + the SDL2 family are built from source). It only needs to be redone
when a dependency changes. If your vcpkg lives somewhere other than `C:/Projects/vcpkg`, pass its
location to Gradle with `-PVCPKG_ROOT=<path>` on the build commands below.

## Build the APK

From the `android/` directory, point `JAVA_HOME` at Android Studio's bundled JDK and build the `dev`
flavor's debug APK:

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
cd C:\Projects\alephone\android
.\gradlew.bat :app:assembleDevDebug
```

The resulting APK is:

```
android\app\build\outputs\apk\dev\debug\app-dev-debug.apk
```

### Product flavors

The project ships one engine as several apps via Gradle **product flavors** (dimension `product`):

| Flavor | applicationId | Data | Build task |
|---|---|---|---|
| `dev` | `org.alephone` | none (push a scenario over `adb`) | `:app:assembleDevDebug` |
| `marathon` | `org.alephone.marathon` | _Marathon_ bundled in the APK | `:app:assembleMarathonRelease` |
| `marathon2` | `org.alephone.marathon2` | _Marathon 2_ bundled in the APK | `:app:assembleMarathon2Release` |
| `infinity` | `org.alephone.infinity` | _Marathon Infinity_ bundled in the APK | `:app:assembleInfinityRelease` |

The `dev` flavor is the everyday development build — it bundles no game data, so you push a scenario
folder over `adb` (see the deploy script below). The three branded flavors are self-contained,
store-installable apps that bundle their base data + scenario inside the APK at build time from
`data/Scenarios/`.

### Signed release APKs

Release builds are signed if `android/keystore.properties` exists (copy
`android/keystore.properties.template`, fill it in, and generate the keystore per that file). Without
it, release builds simply come out unsigned rather than failing. When uploading to the Meta Horizon
Store, **bump `versionCode` in `android/app/build.gradle` before every upload** — the store rejects any
APK whose `versionCode` isn't strictly higher than the last one on that listing.

## Install & run on a Quest

1. Enable **Developer Mode** on the headset (Meta Horizon mobile app → your headset → Developer Mode;
   requires a free Meta developer account / organization), connect over USB-C, and accept the on-headset
   USB-debugging prompt.
2. Confirm the device is visible and install:
   ```powershell
   $adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
   & $adb devices
   & $adb install -r android\app\build\outputs\apk\dev\debug\app-dev-debug.apk
   ```
3. **Put the headset on to give the app focus** — it only runs (and renders) once focused. Launched via
   `adb` without donning the headset, it stays paused.

### One-step deploy (recommended)

`android/deploy.ps1` does the whole loop — build, install, sync a scenario's data to the device
verbatim, force-stop, and relaunch:

```powershell
pwsh android\deploy.ps1                          # build, install, sync "Marathon" (M1), launch
pwsh android\deploy.ps1 -Scenario "Marathon 2"
pwsh android\deploy.ps1 -SkipBuild               # install + sync + launch, no rebuild
pwsh android\deploy.ps1 -NoData                  # code-only redeploy (leave device data alone)
pwsh android\deploy.ps1 -Base                    # also push base data (do once on a fresh device)
```

Scenario data comes from `data/Scenarios/<name>` (populated by the submodule checkout, or download it
from the [Aleph One Scenarios](https://alephone.lhowon.org/scenarios.html) page).

## Debugging

The VR code logs under the `adb logcat` tag **`A1VR`**, and the engine also writes a browsable log file
on the device:

```powershell
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb logcat -c                                 # clear before launch
& $adb logcat -d -s A1VR | Select-Object -Last 40
& $adb shell cat '/sdcard/Android/data/org.alephone/files/Aleph One Log.txt'
```

For deeper background on the port's build system and phased plan, see
[`docs/ANDROID_BUILD.md`](docs/ANDROID_BUILD.md).

---

# Build from source (desktop)

The following are the upstream instructions for the desktop (non-VR) builds. All VR additions are
`__ANDROID__`-guarded, so these continue to work as they do upstream.

## CI status

[![Build Status](https://github.com/Aleph-One-Marathon/alephone/actions/workflows/ci-build.yml/badge.svg)](https://github.com/Aleph-One-Marathon/alephone/actions/workflows/ci-build.yml?query=branch%3Amaster+)

## Scenario data

If you only want an Aleph One executable, you can simply download and untar a release source tarball. However, to build all-in-one Mac apps, flatpaks, or Windows zip files, you will need to populate the data/Scenarios directory. The easiest way to do that is to clone the repository and submodules:

    git clone --recurse-submodules https://github.com/Aleph-One-Marathon/alephone.git

Alternatively, you can download the [data files](https://alephone.lhowon.org/scenarios.html) and unzip them in the data/Scenarios/ directory.

## macOS

These instructions assume familiarity with the Xcode tools and the macOS command line.

macOS dependencies are managed by [vcpkg](https://github.com/microsoft/vcpkg).

Some users have had issues building Aleph One when there are spaces in the path to vcpkg and alephone, so it is recommended to put them in paths without spaces.

Download, bootstrap, and install vcpkg:

    git clone https://github.com/microsoft/vcpkg
    ./vcpkg/bootstrap-vcpkg.sh
    ./vcpkg/vcpkg integrate install

`cd` into Aleph One's vcpkg subdirectory and use the `install-arm-osx.sh` and `install-x64-osx.sh` scripts to install macOS dependencies for arm64 and x64.

You should now be able to open `PBProjects/AlephOne.xcodeproj` in Xcode and build Aleph One.

## Windows

Windows builds are built using [Visual Studio](https://visualstudio.microsoft.com/vs/)

Windows dependencies are managed by [vcpkg](https://github.com/microsoft/vcpkg).

Note this important recommendation in the vcpkg getting-started guide: _If installing globally, we recommend a short install path like: C:\src\vcpkg or C:\dev\vcpkg, since otherwise you may run into path issues for some port build systems._ Spaces in the path and non-ASCII characters can also cause problems. These notes apply to the Aleph One source location as well.

Download, bootstrap, and install vcpkg:

    git clone https://github.com/microsoft/vcpkg.git
    .\vcpkg\bootstrap-vcpkg.bat
    .\vcpkg integrate install

You should now be able to build Aleph One using the `VisualStudio\AlephOne.sln` project file

## Linux/FreeBSD/other

Linux/FreeBSD/other builds are built using autoconf. If you downloaded a source tarball, the configure system is already set up for you. If you cloned from git, you first need to set up the configure system. Install `autoconf` and `autoconf-archive` from your distro package manager, then:

    autoreconf -i

### Dependencies

Aleph One requires a C++17 compiler and the following libraries:

+ `ASIO`
+ `Boost`
+ `SDL2`
+ `SDL2_image`
+ `SDL2_ttf`
+ `zlib`
+ `libsndfile`
+ `openal-soft`

These libraries are recommended for full features and third-party scenario compatibility:

+ `curl` _for stats upload to lhowon.org_
+ `miniupnpc` _for opening router ports_
+ `zziplib` _for using zipped plugins_
+ `vpx` _for film export_
+ `matroska` _for film export_
+ `ebml` _for film export_
+ `vorbis` _for film export_
+ `libyuv` _for film export and video playback_

#### Fedora

First, enable the [RPM Fusion Repository](http://rpmfusion.org/Configuration).

Then, install the following packages.

    sudo dnf install boost-devel curl-devel gcc-c++ \
      libpng-devel SDL2-devel SDL2_ttf-devel SDL2_image-devel asio-devel \
      zziplib-devel miniupnpc-devel openal-soft-devel libsndfile-devel

#### Ubuntu

Run this command to install the necessary prerequisites for building Aleph One:

    sudo apt install build-essential libboost-all-dev libsdl2-dev \
      libsdl2-image-dev libasio-dev libsdl2-ttf-dev libzzip-dev \
      libpng-dev libcurl4-gnutls-dev libminiupnpc-dev libopenal-dev \
      libsndfile1-dev libglu1-dev libvpx-dev libmatroska-dev libebml-dev \
      libvorbis-dev libvorbisenc2 libyuv-dev

### Compile

First, run the configure script:

    ./configure

After running the configure script, start the compile process by running make:

    make

Once the compile is finished, you can install the executable by running:

    sudo make install

By default, the Aleph One executable is installed into `/usr/local/bin/alephone`.

### Run

You can download game data from the [Aleph One Scenarios](https://alephone.lhowon.org/scenarios.html) page. After unzipping one of the games, pass the directory as an argument to Aleph One:

    /usr/local/bin/alephone ~/Games/Marathon
