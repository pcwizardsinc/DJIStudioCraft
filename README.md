# DJIStudioCraft
# OpenOSV Fedora Packaging Kit (DJI Avata 360)

Packaging scripts, patch files, and RPM specifications for building **OpenOSV** natively on **Fedora 44**. 

OpenOSV is a clean-room, open-source toolkit created by [Kemerd](https://github.com/Kemerd) for processing DJI `.OSV` 360° video streams, performing lens calibration, gyro stabilization, horizon locking, and D-Log M color space conversion. This repository provides the necessary tools to compile `osvtool` and `osvgui` (OpenOSV Studio) into a native Fedora RPM package with desktop launcher and GPU acceleration support.

---

## What's Included in `DJIStudio-Craft.zip`

The `DJIStudio-Craft.zip` archive contains all the files required to patch, build, and package OpenOSV for Fedora:

| File | Description |
| :--- | :--- |
| `openosv.spec` | RPM specification file detailing build rules and dependencies. |
| `openosv-0001-fedora-linux.patch` | Patch addressing FFmpeg `pkg-config` targets, TinyEXR links, GTK file dialogs, and font paths. |
| `io.github.kemerd.OpenOSVStudio.desktop` | GNOME application desktop launcher with `RUSTICL_ENABLE=radeonsi` pre-configured. |
| `io.github.kemerd.OpenOSVStudio.svg` | Application icon for the desktop dash. |
| `build-openosv-rpm.sh` | Shell script automating workspace setup and dependency pulling. |

---

## Key Modifications for Fedora

* **FFmpeg Integration:** Configured to locate system FFmpeg via `pkg-config` rather than Windows `vcpkg` defaults.
* **Dear ImGui 1.92.6:** Built directly from source to meet OpenOSV's UI requirements.
* **Native File Dialogs:** Adds Zenity-backed GTK file/folder selection dialogs for GNOME.
* **System Inhibition:** Hooks into `systemd-inhibit` to prevent OS sleep/suspend during heavy video rendering.
* **GPU Acceleration:** Launcher pre-configures Mesa's Rusticl driver for AMD GPUs (e.g., Radeon Pro 580X).

---

## Prerequisites & Installation

Building OpenOSV requires system-level access (`sudo`) and about 10–15 minutes of compile time.

### 1. Extract the Build Kit
Unpack `DJIStudio-Craft.zip` into a working directory:

bash
mkdir -p ~/DJI-Craft && unzip DJIStudio-Craft.zip -d ~/DJI-Craft
cd ~/DJI-Craft
