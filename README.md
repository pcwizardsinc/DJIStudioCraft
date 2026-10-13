# OpenOSV for Fedora: one package for the whole DJI-Craft toolchain

One RPM, `openosv`, installs everything:

| Piece | Installed as |
| --- | --- |
| osvtool (stitch, reframe, `--path` renders) | `/usr/bin/osvtool` |
| OpenOSV Studio (render queue GUI) | `/usr/bin/osvgui`, Activities: **OpenOSV Studio** |
| 360 Director editor | `/usr/share/openosv/director/`, Activities: **360 Director** (also a right-click action on Studio) |
| director-proxy, director-render | `/usr/bin/` |
| Avata 360 D-Log M LUTs | `/usr/share/openosv/luts/` |

Upstream: https://github.com/Kemerd/OpenOSV, pinned to commit
`a00a5b682524b38056330c8d0d70254e16ec01e8` (v0.6.0).

## Kit layout

```
openosv.spec                 the package recipe (version, release, changelog)
patches/openosv-NNNN-*.patch numbered patch series, applied in order
director/                    360 Director files and its launcher
io.github.kemerd.OpenOSVStudio.{desktop,svg}
build-openosv-rpm.sh         one-shot build on Fedora with RPM Fusion
```

Keep this folder in git and tag each release (`git tag 0.9`). That is the
record of what every installed version contained.

## Build and install

```bash
./build-openosv-rpm.sh
sudo dnf install ~/rpmbuild/RPMS/x86_64/openosv-0.6.0-0.9*.rpm
```

`dnf install` of a newer release upgrades the old one in place.

## Patch series

| # | Topic | Upstream-worthy? |
| --- | --- | --- |
| 0001 | Linux build: pkg-config FFmpeg, distro tinyexr, bundled ImGui fallback | yes |
| 0002 | Studio on Linux: zenity dialogs, systemd-inhibit, fonts, app id | yes |
| 0003 | Avata 360 fw 01.00.05.00: extrinsic from Euler angles, refine_far fallback | yes (bug) |
| 0004 | Avata 360: upright levelled view for lens-up/down rigs | yes (bug) |
| 0005 | OpenCL on Mesa rusticl: own sin/cos/tan, build retry | yes |
| 0006 | libx265 default codec on Linux | yes |
| 0007 | `osvtool render --path` (360 Director camera paths) | yes (feature) |

## Making a change (new release)

1. Unpack the pinned upstream source and apply the series as git commits:
   ```bash
   cd ~/rpmbuild/BUILD && rpmbuild -bp ~/rpmbuild/SPECS/openosv.spec
   ```
   or, for a git history to work in:
   ```bash
   git clone https://github.com/Kemerd/OpenOSV && cd OpenOSV
   git checkout a00a5b682524b38056330c8d0d70254e16ec01e8
   git am ~/DJI-Craft/patches/*.patch
   ```
2. Edit, build, test. Commit the change as its own commit.
3. Export only the new commit as the next patch number:
   ```bash
   git format-patch -1 --no-signature -o /tmp && mv /tmp/0001-*.patch ~/DJI-Craft/patches/openosv-0008-short-name.patch
   ```
   (To fix an existing topic instead, amend that commit with
   `git rebase -i` and re-export the whole series with
   `git format-patch --no-signature a00a5b68`.)
4. In `openosv.spec`: add `Patch0008:` with a one-line comment, bump
   `Release:` (0.8 -> 0.9), add a `%changelog` entry
   (`rpmdev-bumpspec -c "what changed" openosv.spec` does both).
5. Run `./build-openosv-rpm.sh`, install, commit and tag.

Changes to 360 Director need no patch: edit the files in `director/`,
bump `Release:`, rebuild.

## Moving to a newer upstream OpenOSV

1. In git: `git fetch && git rebase --onto NEW_COMMIT a00a5b68` with the
   series on a branch. Drop any patch upstream has merged.
2. Re-export with `git format-patch --no-signature NEW_COMMIT`, renumber.
3. In the spec: change `%global commit`, set `Version:` to upstream's,
   reset `Release:` to `0.1`, and check `imgui_ver` against upstream's
   CMake.

## Another machine

The file that travels is the **source RPM** in `~/rpmbuild/SRPMS/`. On any
Fedora (or EL with EPEL) machine with RPM Fusion enabled:

```bash
sudo dnf builddep -y openosv-*.src.rpm && rpmbuild --rebuild openosv-*.src.rpm
```

A built binary RPM only installs on the same Fedora release with RPM
Fusion, because it links that release's FFmpeg libraries. Rebuild after
every Fedora version upgrade.

## Don'ts

- Don't publish the built binary RPM: it links RPM Fusion's GPL FFmpeg.
  Share the spec, patches and director files (this kit) or the SRPM.
- COPR can't host it: COPR builds may not use RPM Fusion's codecs, and
  Fedora's ffmpeg-free can't decode the Avata's HEVC.
- On Fedora 44, don't `dnf swap mesa-va-drivers mesa-va-drivers-freeworld
  --allowerasing` (removes mesa-dri-drivers and gnome-shell), and don't
  use VA-API decode for Avata footage (3840x3840 lenses exceed the
  Polaris decoder).
