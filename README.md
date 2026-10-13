# DJIStudioCraft: 360 video editing for DJI Avata 360 footage on Fedora Linux

DJI Studio, DJI's desktop app for its 360 cameras, runs only on Windows and
macOS. This project packages a Linux replacement for Fedora: one RPM that
stitches, stabilizes, color-converts and reframes `.OSV` 360 video, plus a
keyframe editor for directing camera moves inside the sphere.

It is built on **[OpenOSV](https://github.com/Kemerd/OpenOSV)** by Kemerd, an
independent, clean-room, Apache-2.0 toolkit for DJI's `.OSV` format. This
repository adds a Fedora packaging kit, a patch series for Linux and for
Avata 360 firmware 01.00.05.00, and **360 Director**.

> Not affiliated with or endorsed by DJI. DJI, Avata and Osmo are trademarks
> of SZ DJI Technology Co., Ltd., used here only to describe the footage this
> software works with.

## What you get

One package, `openosv`, installs:

| Piece | What it does | Where |
| --- | --- | --- |
| **osvtool** | Command line: stitch, stabilize, reframe, color-convert, render camera paths | `/usr/bin/osvtool` |
| **OpenOSV Studio** | Desktop app: queue clips and render them with osvtool | Activities: *OpenOSV Studio* |
| **360 Director** | Keyframe editor for camera moves inside 360 video (runs in your browser, offline) | Activities: *360 Director*, or right-click OpenOSV Studio |
| **director-proxy**, **director-render** | Make a light preview copy for 360 Director; render a camera path from a 360 master | `/usr/bin/` |
| Avata 360 D-Log M LUTs | Rec.709, Rec.2100 PQ and HLG, for other editors | `/usr/share/openosv/luts/` |

GPU rendering works on AMD cards through Mesa's OpenCL driver (rusticl). The
Studio launcher turns it on for you.

## Requirements

- **Fedora Workstation** (tested on Fedora 44, GNOME/Wayland), x86_64 or aarch64.
- **[RPM Fusion](https://rpmfusion.org/Configuration) (free)** enabled. Fedora's
  own `ffmpeg-free` can't decode the HEVC streams inside `.OSV` files; RPM
  Fusion's full `ffmpeg` can.
- About 10–15 minutes of build time and `sudo`.
- Optional, for GPU rendering on AMD: `mesa-libOpenCL` (pulled in automatically
  as a recommended package).

Tested on a 2017 iMac (i7-7700K, Radeon Pro 580X, 32 GB) with Avata 360
firmware 01.00.05.00, 8K/60 D-Log M footage.

## Build and install

```bash
git clone https://github.com/pcwizardsinc/DJIStudioCraft.git
cd DJIStudioCraft
./build-openosv-rpm.sh
```

The script checks for RPM Fusion, installs the build tools and dependencies,
downloads the pinned OpenOSV source, applies the patches and builds the RPM.
When it finishes it prints the install command, which looks like:

```bash
sudo dnf install ~/rpmbuild/RPMS/x86_64/openosv-0.6.0-0.9*.rpm
```

To upgrade later: `git pull`, run the script again, and install the new RPM.
`dnf` replaces the old version in place.

If you used an older hand-installed 360 Director (`install.sh`), remove its
per-user copies, or they will override the packaged ones:

```bash
rm -f ~/.local/bin/director-render ~/.local/bin/director-proxy ~/.local/share/applications/io.github.pcwizards.Director360.desktop && rm -rf ~/.local/share/360-director
```

## Using it

### 1. Stitch a 360 master

Render only the stretch you need, because stitching is the slow part.
`--range` is in frames (59.94 per second for 60 fps footage):

```bash
RUSTICL_ENABLE=radeonsi osvtool render MyClip.OSV --mode equirect --size 3840x1920 --fit avata360 --color 709 --range 600-1200 --out MyClip.mp4
```

Leave out `RUSTICL_ENABLE=radeonsi` to render on the CPU, or add it to your
`~/.bashrc`. Add `--stab` for horizon-locked stabilization.

### 2. Make a preview copy

```bash
director-proxy MyClip.mp4
```

This writes `MyClip.proxy.mp4`, a 2048×1024 copy the browser can play smoothly.

### 3. Direct the camera

Open **360 Director** and load `MyClip.proxy.mp4`.

- **Drag** to look around; **scroll** to zoom.
- **Record (R):** the clip plays, and your drags become the camera move.
- **Add keyframe (K):** pins the current view; the camera glides between
  keyframes.
- **Moves:** Look behind, Look up, Look down, Full 360 spin, and
  **Resume forward**, which glides back to the level view straight ahead.
  Each move starts at the playhead from wherever you're looking. Untick
  "Come back to the starting view" to hold a move, then use Resume forward
  when you want to turn back.
- **Set start / Set end (I / O)** choose what gets rendered.
- **Ctrl+Z** undoes. Your work saves automatically in the browser.

### 4. Render

Click **Render video**, type the original `.OSV` file name and the frame your
360 master started at (its `--range` start), then **Save path file**. This
saves `MyClip.path.json`; the video itself is rendered from a terminal with
one of these:

```bash
# Straight from the .OSV: stitches only the frame you see (run in the footage folder)
RUSTICL_ENABLE=radeonsi osvtool render MyClip.OSV --path /path/to/MyClip.path.json --fit avata360 --color 709 --out MyClip.director.mp4

# From the 360 master: faster, because the stitching is already done
director-render /path/to/MyClip.path.json
```

Both keep the clip's audio. Use the same OpenOSV settings (such as `--stab`)
as the 360 master you directed on, so the horizon and heading line up.

Measured speeds on the test iMac: equirect 3840×1920 at about 0.6 fps on the
CPU and 1.5 fps with OpenCL; a 1920×1080 `--path` render at about 3–4 fps
with OpenCL.

## Known issues

- **Render video saves a path file, not a video.** A browser page can't start
  programs, so you run the render command yourself (step 4). The commands
  shown in the dialog assume your browser saves to `~/Downloads`; change the
  path if it saves elsewhere. Planned for 0.10: a Render button that starts
  the render directly.
- **Path files saved by 360 Director before 0.9** may have no range end. With
  those, `osvtool --path` renders to the end of the whole `.OSV`. Add
  `--range` to the command, or open the project in 0.9 and save the path
  file again.
- **`osvtool selfcheck` reports one failure** (seam NCC 0.756 vs 0.8) on Avata
  360 footage. The threshold is tuned for the Osmo 360; renders are fine.
- **Hardware video decoding (VA-API) is not used.** The Avata 360's
  3840×3840 lens streams are too large for many GPUs' decoders, so decoding
  runs on the CPU and usually sets the overall speed.
- **Flatpak browsers:** 360 Director opens from `/usr/share`, which a Flatpak
  browser may not be able to read. A Fedora RPM browser (the default Firefox)
  works. Not yet tested with Flatpak browsers.

## Release history

Versions are `0.6.0-0.N`: OpenOSV upstream v0.6.0, Fedora release N.

| Release | Changes |
| --- | --- |
| 0.9 | 360 Director: **Resume forward** move; path files always record the range end |
| 0.8 | One package: 360 Director moved in from `~/.local`, Studio gets a "360 Director" action; patch split into a seven-patch series |
| 0.7 | `osvtool render --path` / `--path-start`: render a 360 Director camera path |
| 0.6 | OpenCL: own sin/cos/tan, since Fedora 44's libclc can't link them on Polaris GPUs |
| 0.5 | `libx265` default encoder on Linux; OpenCL build retry for Mesa rusticl |
| 0.4 | Avata 360: levelled renders come out right side up |
| 0.3 | Avata 360 firmware 01.00.05.00: lens extrinsics from Euler angles; refined-far calibration fallback |
| 0.2 | Build fix: `miniz-devel` |
| 0.1 | First Fedora package: osvtool and OpenOSV Studio with Linux/GNOME support |

Full notes are in the `%changelog` section of `openosv.spec`.

## For maintainers

### Repository layout

```
openosv.spec                    the package recipe (version, release, changelog)
patches/openosv-NNNN-*.patch    patch series against upstream, applied in order
director/                       360 Director: page, tools, launcher, README
io.github.kemerd.OpenOSVStudio.{desktop,svg}   Studio launcher and icon
build-openosv-rpm.sh            one-shot build on Fedora with RPM Fusion
```

Upstream is pinned to OpenOSV commit
`a00a5b682524b38056330c8d0d70254e16ec01e8` (v0.6.0).

### Patch series

| # | Topic | Upstream-worthy? |
| --- | --- | --- |
| 0001 | Linux build: pkg-config FFmpeg, distro tinyexr, bundled ImGui fallback | yes |
| 0002 | Studio on Linux: zenity dialogs, systemd-inhibit, fonts, app id | yes |
| 0003 | Avata 360 fw 01.00.05.00: extrinsic from Euler angles, refine_far fallback | yes (bug) |
| 0004 | Avata 360: upright levelled view for lens-up/down rigs | yes (bug) |
| 0005 | OpenCL on Mesa rusticl: own sin/cos/tan, build retry | yes |
| 0006 | libx265 default codec on Linux | yes |
| 0007 | `osvtool render --path` (360 Director camera paths) | yes (feature) |

### Making a change

`KIT` below is your clone of this repository, for example `KIT=~/DJIStudioCraft`.

1. Get the patched source as a git history to work in:
   ```bash
   git clone https://github.com/Kemerd/OpenOSV && cd OpenOSV
   git checkout a00a5b682524b38056330c8d0d70254e16ec01e8
   git am $KIT/patches/*.patch
   ```
2. Edit, build, test, and commit the change as its own commit.
3. Export it as the next patch number:
   ```bash
   git format-patch -1 --no-signature -o /tmp && mv /tmp/0001-*.patch $KIT/patches/openosv-0008-short-name.patch
   ```
   To fix an existing topic instead, amend that commit with `git rebase -i`
   and re-export the whole series with `git format-patch --no-signature a00a5b68`.
4. In `openosv.spec`, add `Patch0008:` with a one-line comment, bump
   `Release:`, and add a `%changelog` entry (`rpmdev-bumpspec -c "what changed"
   openosv.spec` does both).
5. Build, install and test, then commit, tag and push:
   ```bash
   git commit -am "openosv 0.6.0-0.10: what changed" && git tag 0.10 && git push origin main 0.10
   ```

Changes to 360 Director need no patch: edit `director/`, bump `Release:`,
rebuild.

### Moving to a newer upstream OpenOSV

1. On a branch holding the series, `git fetch` upstream and
   `git rebase --onto NEW_COMMIT a00a5b68`. Drop any patch upstream has merged.
2. Re-export with `git format-patch --no-signature NEW_COMMIT` and renumber.
3. In the spec, change `%global commit`, set `Version:` to upstream's, reset
   `Release:` to `0.1`, and check `imgui_ver` against upstream's CMake.

### Other machines

The build script produces a **source RPM** in `~/rpmbuild/SRPMS/`. It rebuilds
on any Fedora (or EL with EPEL) machine with RPM Fusion enabled:

```bash
sudo dnf builddep -y openosv-*.src.rpm && rpmbuild --rebuild openosv-*.src.rpm
```

A built binary RPM installs only on the same Fedora release with RPM Fusion,
because it links that release's FFmpeg libraries. Rebuild after every Fedora
version upgrade.

### Don'ts

- **Don't publish built binary RPMs** (here, in Releases, or elsewhere). They
  link RPM Fusion's GPL FFmpeg. Share this repository or the source RPM.
- **COPR can't host it:** COPR builds may not use RPM Fusion's codecs, and
  Fedora's `ffmpeg-free` can't decode the `.OSV` HEVC streams.
- On Fedora 44, don't run `dnf swap mesa-va-drivers mesa-va-drivers-freeworld
  --allowerasing`: it removes `mesa-dri-drivers` and `gnome-shell`.

## License

OpenOSV is Apache-2.0 (see upstream). The bundled Dear ImGui is MIT. The
packaging kit, patches and 360 Director in this repository are offered under
Apache-2.0, matching OpenOSV.

## Credits

- **[Kemerd](https://github.com/Kemerd)** for OpenOSV, which does the real work.
- **[Dear ImGui](https://github.com/ocornut/imgui)** by Omar Cornut.
- Fedora packaging, Avata 360 fixes and 360 Director by Andrew Chadick
  ([PC Wizards Inc.](https://www.pcwizardsinc.com)).
