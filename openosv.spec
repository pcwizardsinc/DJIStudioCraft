# OpenOSV for Fedora: osvtool (CLI) + OpenOSV Studio (GUI) + 360 Director
# Built from a pinned upstream commit plus a numbered Linux/Avata patch series.
# Maintenance notes (adding a patch, moving to a new upstream commit,
# rebuilding on another machine) are in README.md beside this spec.

%global commit      a00a5b682524b38056330c8d0d70254e16ec01e8
%global shortcommit %(c=%{commit}; echo ${c:0:7})
%global imgui_ver   1.92.6
%global app_id      io.github.kemerd.OpenOSVStudio
%global dir_id      io.github.pcwizards.Director360

Name:           openosv
Version:        0.6.0
Release:        0.9.git%{shortcommit}%{?dist}
Summary:        Stitch, reframe, direct and color-convert DJI Avata 360 / Osmo 360 (.OSV) footage

# OpenOSV is Apache-2.0; the bundled Dear ImGui is MIT. 360 Director is
# Andrew Chadick's own work, shipped under OpenOSV's Apache-2.0.
License:        Apache-2.0 AND MIT
URL:            https://github.com/Kemerd/OpenOSV
Source0:        https://github.com/Kemerd/OpenOSV/archive/%{commit}/OpenOSV-%{commit}.tar.gz
# Fedora's imgui is older than the 1.92 API osvgui needs, so build it in.
Source1:        https://github.com/ocornut/imgui/archive/v%{imgui_ver}/imgui-%{imgui_ver}.tar.gz
Source2:        %{app_id}.desktop
Source3:        %{app_id}.svg
# 360 Director: keyframe camera paths for 360 video (browser editor + tools)
Source10:       360-director.html
Source11:       director-render.py
Source12:       director-proxy.sh
Source13:       %{dir_id}.desktop
Source14:       README-director.md

# Patch series, applied in order. One topic per patch so each can be rebased
# on its own and offered upstream as a separate change.
# Linux support: pkg-config FFmpeg, distro tinyexr, bundled ImGui fallback.
Patch0001:      openosv-0001-linux-build.patch
# Studio: zenity (GTK) file dialogs, systemd-inhibit, Fedora fonts, GNOME app id.
Patch0002:      openosv-0002-studio-linux.patch
# Avata 360 fw 01.00.05.00: lens extrinsic from Euler angles; native_refine_far.
Patch0003:      openosv-0003-avata-fw-01.00.05.00.patch
# Avata 360: levelled view of lens-up/lens-down rigs right side up.
Patch0004:      openosv-0004-avata-orientation.patch
# OpenCL on Mesa rusticl / unpatched libclc (Polaris): own trig, build retry.
Patch0005:      openosv-0005-opencl-rusticl.patch
# osvtool: libx265 default on Linux (no NVENC).
Patch0006:      openosv-0006-libx265-default.patch
# osvtool render --path / --path-start (360 Director camera paths).
Patch0007:      openosv-0007-camera-path.patch

ExclusiveArch:  x86_64 aarch64

BuildRequires:  cmake >= 3.28
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  pkgconf-pkg-config
# RPM Fusion's full FFmpeg: Fedora's ffmpeg-free lacks the HEVC support
# that .OSV files (two HEVC streams) need.
BuildRequires:  ffmpeg-devel
BuildRequires:  fmt-devel
BuildRequires:  spdlog-devel
BuildRequires:  json-devel
BuildRequires:  cli11-devel
BuildRequires:  tinyexr-devel
# tinyexr-devel's CMake config links -lminiz but does not pull in the .so symlink.
BuildRequires:  miniz-devel
BuildRequires:  glfw-devel
BuildRequires:  mesa-libGL-devel
BuildRequires:  opencl-headers
BuildRequires:  ocl-icd-devel
BuildRequires:  desktop-file-utils

# osvtool writes MP4s by piping frames to the ffmpeg program (x265 encoder).
Requires:       ffmpeg
# GTK file chooser for the GUI's "Add files / Add folder" buttons.
Requires:       zenity
Requires:       dejavu-sans-fonts
Requires:       dejavu-sans-mono-fonts
Requires:       hicolor-icon-theme
# GPU rendering on AMD (Radeon Pro 580X etc.) through Mesa rusticl.
Recommends:     mesa-libOpenCL
# 360 Director: director-render plays a camera path over the 360 master;
# the editor itself opens in the default web browser.
Requires:       python3
Requires:       python3-numpy
Requires:       python3-opencv
Requires:       xdg-utils

Provides:       bundled(imgui) = %{imgui_ver}

%description
OpenOSV is an independent, clean-room, open-source toolkit for DJI's .OSV
360 video format (Avata 360, Osmo 360). It parses the container, reads the
embedded lens calibration and IMU data, stitches and reframes the two
fisheye streams on the CPU or GPU (OpenCL), stabilizes with horizon lock,
and converts 10-bit D-Log M to Rec.2100 PQ / HLG or Rec.709.

This package contains osvtool (command line), OpenOSV Studio (osvgui), a
desktop app that queues clips and drives osvtool, and 360 Director, a
keyframe editor for camera moves inside 360 footage (director-proxy,
director-render, and a browser-based editor). It also ships the Avata 360
D-Log M LUTs for use in other editors.

%prep
%autosetup -n OpenOSV-%{commit} -p1 -a 1

%build
%cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DOSV_ENABLE_CUDA=OFF \
    -DOSV_ENABLE_OPENCL=ON \
    -DOSV_ENABLE_METAL=OFF \
    -DOSV_BUILD_TESTS=OFF \
    -DOSV_BUILD_OFX=OFF \
    -DOSV_BUILD_PREMIERE=OFF \
    -DOSV_BUILD_TOOLS=ON \
    -DOSV_BUILD_GUI=ON \
    -DOSV_IMGUI_SOURCE_DIR=%{_builddir}/OpenOSV-%{commit}/imgui-%{imgui_ver}
%cmake_build --target osvtool osvgui

%install
install -Dpm 0755 %{__cmake_builddir}/bin/osvtool %{buildroot}%{_bindir}/osvtool
install -Dpm 0755 %{__cmake_builddir}/bin/osvgui  %{buildroot}%{_bindir}/osvgui
install -Dpm 0644 %{SOURCE2} %{buildroot}%{_datadir}/applications/%{app_id}.desktop
install -Dpm 0644 %{SOURCE3} %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/%{app_id}.svg
# Avata 360 LUTs only (the full set is ~170 MB; `osvtool lut` makes any other).
for d in Rec2100_PQ Rec2100_HLG Rec709; do
    install -d %{buildroot}%{_datadir}/%{name}/luts/$d
    install -pm 0644 luts/$d/DJI_Avata360_* %{buildroot}%{_datadir}/%{name}/luts/$d/
done
install -pm 0644 luts/README.txt %{buildroot}%{_datadir}/%{name}/luts/README.txt

# 360 Director
install -Dpm 0644 %{SOURCE10} %{buildroot}%{_datadir}/%{name}/director/360-director.html
install -Dpm 0755 %{SOURCE11} %{buildroot}%{_bindir}/director-render
install -Dpm 0755 %{SOURCE12} %{buildroot}%{_bindir}/director-proxy
sed -i '1s|^#!.*python3.*|#!/usr/bin/python3|' %{buildroot}%{_bindir}/director-render
install -Dpm 0644 %{SOURCE13} %{buildroot}%{_datadir}/applications/%{dir_id}.desktop
install -pm 0644 %{SOURCE14} README-director.md

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{app_id}.desktop
desktop-file-validate %{buildroot}%{_datadir}/applications/%{dir_id}.desktop
# Maths, colour pipeline and LUT round-trip; needs no footage or GPU.
%{buildroot}%{_bindir}/osvtool selfcheck

%files
%license LICENSE NOTICE
%doc README.md CHANGELOG.md docs/FORMAT.md docs/COLOR.md docs/STUDIO.md
%doc README-director.md
%{_bindir}/osvtool
%{_bindir}/osvgui
%{_bindir}/director-render
%{_bindir}/director-proxy
%{_datadir}/applications/%{app_id}.desktop
%{_datadir}/applications/%{dir_id}.desktop
%{_datadir}/icons/hicolor/scalable/apps/%{app_id}.svg
%{_datadir}/%{name}/

%changelog
* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.9.gita00a5b6
- 360 Director: "Resume forward" move glides back to the level view
  straight ahead, turning back the way the camera came
- 360 Director: path files always record the range end, so osvtool --path
  no longer runs on to the end of the whole .OSV when Set end wasn't used

* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.8.gita00a5b6
- One package for everything: 360 Director (editor, director-proxy,
  director-render) now ships here instead of ~/.local; Studio's launcher
  gains a "360 Director" action
- Split the single Fedora patch into a seven-patch topic series (same
  resulting source tree as 0.7)

* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.7.gita00a5b6
- osvtool render --path: animate the reframe view from a 360 Director
  camera path (.path.json); --path-start places its time 0 on the clip

* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.6.gita00a5b6
- OpenCL: self-contained sin/cos/tan in the kernel preamble; Fedora 44's
  unpatched libclc fails to link the driver trig builtins on Polaris

* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.5.gita00a5b6
- osvtool: default --codec libx265 on Linux (hevc_nvenc is NVIDIA-only)
- OpenCL: retry the kernel build without strict divide/sqrt when Mesa
  rusticl cannot link them (Fedora's unpatched libclc)

* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.4.gita00a5b6
- Avata 360: half turn about the levelled view's forward axis, so horizon-
  levelled renders come out right side up (verified as --roll 180)

* Mon Oct 12 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.3.gita00a5b6
- Avata 360 fw 01.00.05.00: derive cam_extri_q from Euler angles; use the
  native_refine_far calibration pair when the native pairs are empty

* Fri Oct 09 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.2.gita00a5b6
- BuildRequires miniz-devel (link failure: cannot find -lminiz)

* Fri Oct 09 2026 Andrew Chadick <andrew@localhost> - 0.6.0-0.1.gita00a5b6
- Initial Fedora package: osvtool + OpenOSV Studio with Linux/GNOME patch
