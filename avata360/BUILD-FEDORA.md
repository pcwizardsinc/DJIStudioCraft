# Building Avata 360 Editor on Fedora 45

How to compile Avata 360 Editor natively on Fedora 45 Workstation. It's built on [OpenOSV](https://github.com/Kemerd/OpenOSV), the open-source DJI 360 `.OSV` stitcher, which upstream builds only on Windows and macOS through vcpkg. The `djicraft` branch of [pcwizardsinc/DJIStudioCraft](https://github.com/pcwizardsinc/DJIStudioCraft) (a fork of OpenOSV) adds what Linux needs.

Based on upstream commit `d46b2bc` (v0.6.4, Oct 26 2026).

Verified with GCC 14 against FFmpeg 9.0.2 (the version RPM Fusion ships for Fedora 45): everything builds with no errors and all 596 tests pass. The build was done on a non-Fedora machine with the same library versions, so treat the first Fedora build as the real confirmation.

## What you get

| Program | What it is |
|---|---|
| `osvtool` | Command line: probe, stitch, reframe, grade (D-Log M to Rec.709 / HLG / PQ) and render `.OSV` clips to MP4 or stills |
| `osvgui` | OpenOSV Studio: drag-and-drop desktop app that queues clips and drives `osvtool` |

Not included: the Premiere Pro, DaVinci Resolve (OpenFX) and VEGAS plug-ins. Upstream builds those for Windows and macOS only.

## What the `djicraft` branch changes

| File | Change |
|---|---|
| `cmake/OsvFFmpeg.cmake` | Finds FFmpeg through pkg-config on Linux instead of vcpkg's `FindFFMPEG` |
| `CMakeLists.txt` | Accepts Fedora's `tinyexr::tinyexr` target name; builds Dear ImGui on Linux; adds `cmake --install` rules |
| `cmake/OsvImGui.cmake` (new) | Builds Dear ImGui v1.92.9b (the version upstream pins) with its GLFW + OpenGL 3 backends, SHA-256 checked |
| `tools/osvgui/Platform.cpp`, `Platform.h` | Real file dialogs on Linux through zenity (GNOME) or kdialog (KDE). Upstream had empty stubs, so "Add clips" did nothing |
| `packaging/linux/io.github.kemerd.OpenOSV.desktop` (new) | Application-menu launcher for OpenOSV Studio |
| `tests/unit/test_video.cpp` | Makes the `osvtool` command-line tests portable. They used Windows-only shell idioms (outer quotes for `cmd.exe`, `> nul`, raw `std::system()` return values), so they failed on Linux even though `osvtool` worked |

## 1. RPM Fusion and full FFmpeg

Fedora's stock `ffmpeg-free` has no HEVC decoder, and the Osmo 360 and Avata 360 both record HEVC. RPM Fusion's `ffmpeg` (9.0.2 on Fedora 45) has it, plus the x264/x265 encoders the render step uses.

Use `dnf install` for the release packages. On Fedora 45, `rpm -Uvh` refuses them with a NOKEY error.

```
sudo dnf install https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm https://mirrors.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm
```

```
sudo dnf swap ffmpeg-free ffmpeg --allowerasing
```

Check that the HEVC decoder is now present:

```
ffmpeg -hide_banner -decoders 2>/dev/null | grep -w hevc
```

## 2. Build dependencies

```
sudo dnf install gcc-c++ cmake ninja-build git pkgconf-pkg-config ffmpeg-devel fmt-devel spdlog-devel cli11-devel tinyexr-devel catch-devel ocl-icd-devel opencl-headers glfw-devel zenity clinfo 'cmake(nlohmann_json)' 'pkgconfig(gl)'
```

Use `catch-devel` (Catch2 v3). `catch2-devel` is the old Catch2 v2 and the tests will not compile against it.

On KDE Plasma, add `kdialog`. OpenOSV Studio prefers it there and falls back to zenity.

## 3. Get the source

```
mkdir -p ~/DJI-Craft && cd ~/DJI-Craft && git clone -b djicraft https://github.com/pcwizardsinc/DJIStudioCraft OpenOSV && cd OpenOSV
```

This clones the fork's `djicraft` branch into `~/DJI-Craft/OpenOSV`, with every Linux fix already applied. To pick up later changes, run `git pull` inside that folder and rebuild.

## 4. Configure and build

The first configure downloads Dear ImGui from GitHub, so it needs internet access.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOSV_ENABLE_CUDA=OFF -DOSV_BUILD_TESTS=OFF
```

The configure summary at the end should show:

```
--   OpenCL backend : ON
--   Studio (GUI)   : ON
```

Then compile. This is roughly 250 C++ files and takes a while; Ninja uses every core.

```
cmake --build build
```

The results are `build/bin/osvtool` and `build/bin/osvgui`.

## 5. Check it

```
./build/bin/osvtool --version
```

You should see lines like these:

```
OpenOSV 0.6.4
  modules : core container meta color video geom render_cpu render_opencl io
  ffmpeg  : avcodec 63.x / avformat 63.x / avutil 61.x (GPL build - not redistributable with OpenOSV)
  opencl  : <your GPU>
```

The "GPL build" note is expected with RPM Fusion's FFmpeg. It only matters if you redistribute binaries; see Licensing below.

If the `opencl` line says `unavailable`, see the next section. Rendering still works on the CPU, just slower.

Start OpenOSV Studio:

```
./build/bin/osvgui
```

## 6. Install (optional)

This puts both programs in `/usr/local/bin`, adds "OpenOSV Studio" to the application menu, and copies the licence files to `/usr/local/share/doc/openosv`.

```
sudo cmake --install build
```

To remove it later, delete the files listed in `build/install_manifest.txt`:

```
sudo xargs rm -v < build/install_manifest.txt
```

## GPU acceleration (OpenCL)

OpenOSV renders through OpenCL on Linux. Install the runtime for your GPU, then run `clinfo -l` to confirm a device shows up.

| GPU | Package | Notes |
|---|---|---|
| NVIDIA | `xorg-x11-drv-nvidia-cuda` | Needs the RPM Fusion `akmod-nvidia` driver, not nouveau |
| AMD Radeon | `mesa-libOpenCL` | Run with `RUSTICL_ENABLE=radeonsi` set, e.g. `RUSTICL_ENABLE=radeonsi osvgui` |
| Intel | `intel-compute-runtime` | |

CUDA is left off (`-DOSV_ENABLE_CUDA=OFF`). NVIDIA's `nvcc` usually does not support the newest GCC Fedora ships, and NVIDIA cards run the OpenCL path fine.

## Using it

Render a clip to a 4K equirectangular 360 video:

```
osvtool render CAM_0001.OSV --all --mode equirect --size 3840x1920 --stab horizon --color 709 --out CAM_0001_360.mp4
```

Render a reframed flat 1080p view (turn 90° right, tilt 10° down):

```
osvtool render CAM_0001.OSV --all --yaw 90 --pitch -10 --fov 100 --size 1920x1080 --stab full --color 709 --out CAM_0001_side.mp4
```

Batch every clip in the current folder:

```
for f in *.OSV; do osvtool render "$f" --all --mode equirect --size 3840x1920 --stab horizon --color 709 --out "${f%.OSV}_360.mp4"; done
```

See what's in a clip without rendering it:

```
osvtool probe CAM_0001.OSV
```

`osvtool --help` lists every command and `osvtool render --help` every render option.

## Running the test suite (optional)

```
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DOSV_ENABLE_CUDA=OFF -DOSV_BUILD_TESTS=ON
cmake --build build-tests
cd build-tests && ctest --output-on-failure
```

One failure is expected: `FFmpeg build is LGPL and reports its version`. It checks that FFmpeg is an LGPL build, and RPM Fusion's is GPL (it includes x264 and x265). Nothing is broken. Every other test should pass; about 130 report "Skipped" because they need a real `.OSV` sample clip, which is not in the repository. To run those too, copy one of your clips to `example_footage_dlogm.OSV` in the source folder before configuring, or point `-DOSV_SAMPLE_FILE=/path/to/clip.OSV` at it.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `Package 'libavcodec' ... not found` at configure | `ffmpeg-devel` is not installed, or the swap in step 1 did not happen. `rpm -q ffmpeg ffmpeg-devel` should show RPM Fusion packages |
| `Could not find a package configuration file provided by "tinyexr"` | `sudo dnf install tinyexr-devel` |
| Catch2 errors such as `catch2/catch_test_macros.hpp: No such file` | `catch2-devel` (v2) is installed instead of `catch-devel` (v3). `sudo dnf swap catch2-devel catch-devel` |
| Configure fails downloading Dear ImGui | No internet during configure. Download `https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz`, unpack it, and add `-DOSV_IMGUI_SOURCE_DIR=/path/to/imgui-1.92.9b` |
| `Studio (GUI) : OFF` in the configure summary | `glfw-devel` or the OpenGL headers are missing. Re-run step 2 |
| "Add clips" button missing or does nothing in OpenOSV Studio | zenity (GNOME) or kdialog (KDE) is not installed. Drag and drop onto the window works regardless |
| Render fails with an FFmpeg/encoder error | Rendering to MP4 pipes frames into the `ffmpeg` command. `ffmpeg -encoders \| grep -E 'libx265\|hevc_'` should list an HEVC encoder |
| HEVC decode errors on every clip | Still on `ffmpeg-free`. Redo step 1 |

## Licensing

OpenOSV is Apache-2.0. RPM Fusion's FFmpeg is a GPL build, and binaries linked against it are not redistributable as Apache-2.0 alone. Building and using it yourself is fine. If you ship an RPM, keep FFmpeg as a dependency pulled from RPM Fusion rather than bundling it, and read OpenOSV's `NOTICE` file.
