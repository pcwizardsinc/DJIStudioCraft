# Avata 360 Editor: Roadmap

**Avata 360 Editor** is a single desktop application for Fedora Workstation (44/45) that imports DJI Avata 360 video, lets you reframe, keyframe, grade and track it on a timeline, and exports finished MP4s and edit-ready files for other editors. It runs on any Fedora computer, whatever its graphics (AMD, Intel or NVIDIA), and ships as one RPM. The working directory and repository keep the name DJI-Craft / DJIStudioCraft.

Built by extending **OpenOSV Studio** (`tools/osvgui` in [Kemerd/OpenOSV](https://github.com/Kemerd/OpenOSV), Apache-2.0) into a full editor on top of the OpenOSV engine. Personal project, owned by andrew@chadick.org.

This file is the project's source of truth. Update it at the end of every working session.

---

## Scope

**In scope: every way the Avata 360 records video.**

| Recording setting | Values | What the editor must do |
|---|---|---|
| Lens mode | 360° (`.OSV`, both lenses) / Single Lens (one lens, ordinary H.265 `.MP4`, 3840×2880 4:3, up to 60 fps) | Stitch and reframe `.OSV`; edit Single Lens `.MP4` as flat video |
| 360° resolution | 8K 7680×3840, 6K 6000×3000 (24–60 fps per DJI's spec sheet), plus the 4K-per-lens 100 fps mode found on the DangTree clip | Read the mode from each clip's metadata; never assume one |
| Colour mode | Normal, D-Log M (10-bit) | Both input paths: D-Log M through the Avata fit, Normal passed through |
| Gimbal mode (360°) | Follow (horizon stays level) / FPV (view rolls with the aircraft) | Offer both as the default framing; read which one was used if the clip records it |
| Pilot's view while recording | Pan, tilt, zoom/FOV and roll set live with the dials; asteroid appears at max FOV looking down | DJI plays clips back from "the original flight view" by default, so this is likely in the metadata: import it as the starting camera track |
| View styles at edit time | Asteroid (tiny planet), ultra-wide, wide, selfie, dewarp, flat | These are reframing choices, not recording modes: a 360° clip holds the whole sphere |
| Lens accessories | Bare lenses, lens guards, underwater (calibration sets in the clip) | Expose the engine's calibration choice |
| Proxy | `.LRF` beside each `.OSV` | Preview from it automatically |

**Out of scope:** photos (360° JPEG/DNG), other DJI cameras (the Osmo 360 keeps working in the engine but isn't tested or exposed), and the goggles' own liveview recordings.

## Decisions

| Decision | Detail |
|---|---|
| Name | Avata 360 Editor |
| Approach | Extend OpenOSV Studio into the full editor; one application |
| Footage | Avata 360 video only, all of its recording modes |
| Platform | Any Fedora Workstation computer, any GPU vendor; CPU-only must still work |
| Fixes | Go both places: committed to the DJIStudioCraft fork and offered upstream to OpenOSV as pull requests |
| Ownership | Personal project, andrew@chadick.org |

## Graphics support plan

The app has to run well on AMD, Intel and NVIDIA, and still run (slower) with no usable GPU.

| Job | AMD | Intel | NVIDIA | No GPU |
|---|---|---|---|---|
| Stitch / render | OpenCL via Mesa Rusticl (`mesa-libOpenCL`) | OpenCL via `intel-compute-runtime` or Rusticl | OpenCL from the proprietary driver (RPM Fusion `xorg-x11-drv-nvidia-cuda`) | Engine's CPU renderer |
| Video decode | VA-API (to add) + RPM Fusion `mesa-va-drivers-freeworld` | VA-API (to add) via `intel-media-driver` | NVDEC through FFmpeg's CUDA path (the engine already tries it) | Software |
| Video encode | VA-API H.264/H.265 | VA-API H.264/H.265 | NVENC | libx264 / libx265 |

Rules that follow from this:

- **One GPU path, OpenCL, for every vendor.** The CUDA renderer stays off in the RPM build: NVIDIA's compiler trails Fedora's GCC, and OpenCL already runs on NVIDIA cards.
- **The app configures itself.** It sets `RUSTICL_ENABLE` before loading OpenCL, so nobody has to edit environment files, and it shows which device it's using.
- **Always a fallback.** Every hardware path falls back to software without failing the edit.
- **Test machines:** the iMac (AMD Polaris), a laptop with an NVIDIA GTX 1650 Ti, and an AMD-only laptop; a CPU-only run as well.

## Status

**Last updated:** 2026-11-09

| Area | State |
|---|---|
| Engine builds on Fedora 45 (GCC 16.2.1, FFmpeg 9.0.2 from RPM Fusion) | Done |
| Engine reads Avata 360 `.OSV` and `.LRF` | Done, verified on the DangTree clip |
| Stitch quality on 4K-mode footage | Looks clean at preview size; full-resolution check pending |
| D-Log M to Rec.709 colour | Works; Avata fit (`--fit avata360`) must be chosen by hand today |
| Horizon levelling on Avata 360 | Fixed on `djicraft`, verified on frames 300 and 4300 |
| GPU rendering (OpenCL) | Detected on AMD (iMac, Radeon Pro 580X via Rusticl, needs `RUSTICL_ENABLE=radeonsi`) and NVIDIA (laptop, GTX 1650 Ti, proprietary driver 615.78.08). NVIDIA end to end on DangTree: NVDEC decode, OpenCL render, NVENC HEVC 10-bit encode |
| Hardware video decoding on Linux | NVDEC confirmed on NVIDIA; VA-API confirmed on the iMac's Radeon (`decoding with vaapi`) |
| Editor (timeline, preview, keyframes) | Not started |
| RPM | Not started |

## Working setup

| Item | Detail |
|---|---|
| Build machine | 2017 27" iMac (iMac18,3), Fedora 45, AMD Radeon Pro 580X |
| Working directory | `~/DJI-Craft` |
| Repository | [pcwizardsinc/DJIStudioCraft](https://github.com/pcwizardsinc/DJIStudioCraft), a fork of OpenOSV; work on branch `djicraft` |
| Local clone | `~/DJI-Craft/OpenOSV` (remotes: `origin` = the fork, `upstream` = Kemerd/OpenOSV) |
| Based on | upstream commit `d46b2bc` (v0.6.4) |
| Project docs | `avata360/` in the repo: this file and `BUILD-FEDORA.md` |
| Test footage | `~/DJI-Craft/footage/DangTree/` |
| Build | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOSV_ENABLE_CUDA=OFF -DOSV_BUILD_TESTS=OFF`, then `cmake --build build` |
| Before running | `conda deactivate`, and `RUSTICL_ENABLE=radeonsi` (set permanently in `~/.config/environment.d/rusticl.conf`) |
| Full build instructions | `BUILD-FEDORA.md` |

### How work moves

Development happens in chat with Claude on a separate machine. Claude compiles and runs the test suite on its own Linux build box, then hands over patches. The iMac applies them (`git apply`), builds, runs the real tests on footage, commits to `djicraft` and pushes to the fork. Upstream pull requests go from separate one-topic branches, never from `djicraft` itself. Output images come back as JPEGs attached directly to chat, because the Drive connection can't show image content. Anything with a window can only be tested on the iMac.

## Test footage

**DangTree:** `DJI_20261018153702_0001_D_2026`, recorded 2026-10-18.

| File | What it is |
|---|---|
| `.OSV` (2.45 GB) | Master: two 1920×1920 lens streams, HEVC 10-bit D-Log M, **100 fps, 4K mode (not 8K)**, 85.5 s, 8,552 frames |
| `.LRF` (149 MB) | Proxy: 1920×960 side-by-side, 25 fps, 8-bit |
| `.SRT` (1.3 MB) | DJI per-frame telemetry |
| `No_Human_Hands.mp3` | Music for the edit |

Camera: Avata 360, firmware 01.00.06.00. Stitched output tops out at 3840×1920 for this clip.

**Still needed:** a short 8K clip (10–20 s, something with straight lines). It's the only way to check the 8K path, and 8K mode doesn't depend on the unverified 4K crop rule.

## Known issues

1. **Avata levelling (fixed on `djicraft`).** On this clip the engine read the drone's nose as pointing at the ground while it flew level, a constant 90° pitch error. The fix measures the right correction per clip (tests 0, +90, −90, 180°; picks the one that makes the drone's up vertical on average; keeps upstream's reading when it already works). Worth reporting upstream with the evidence.
2. **4K-mode lens scaling is a guess.** The engine assumes a crop derived from 6K clips for 4K-mode footage. The stitch looks clean at preview size; check a full-resolution crop of the seam before relying on it.
3. **No VA-API decoding.** The engine tries Windows D3D11 and NVIDIA CUDA, then falls back to CPU. AMD and Intel on Linux decode through VA-API, which also needs RPM Fusion's `mesa-va-drivers-freeworld` (Fedora disables HEVC in its own Mesa).
4. **Avata colour fit is manual.** The default curve is the Osmo 360's. The engine knows the camera model, so it should pick `avata360` automatically.
5. **100 fps sources.** 100 doesn't divide into 60. The editor needs 50 fps playback/export and slow-motion options (25/30 fps conform).
6. **Rusticl libclc warning.** Fedora's libclc lacks Rusticl's recommended patches. Harmless so far; if a render shows wrong colours or blocky patches, suspect this first and compare with a CPU render.
7. **Free DaVinci Resolve on Linux can't import H.264/H.265.** Exports meant for Resolve need DNxHR (or ProRes) in `.mov`.
8. **Encoder was NVIDIA-only (fixed, `phase1-encoder.patch`).** `osvtool` hard-coded `hevc_nvenc`, which loads but fails to open on AMD/Intel machines a second too late for the pipe's fallback to catch, so exports produced no file on the iMac. `--codec` now defaults to `auto`: the tool probes hevc_nvenc, hevc_vaapi, hevc_qsv, hevc_amf, hevc_videotoolbox, libx265 and uses the first that runs. VA-API and QSV encode flags added to the pipe.
9. **OpenCL render crashes on the iMac's Radeon** (`nir_shader not fully linked`) and falls back to CPU. A Rusticl/libclc compiler bug on the engine's GPU kernels; needs debugging on AMD hardware, not reproducible without a GPU. The render still completes on the CPU. Investigate in Phase 1.
10. **Render speed is low.** 4K 360° export of DangTree on the GTX 1650 Ti laptop: about 28 s of one-time clip analysis, then 1.8 fps (the 85 s clip would take about 80 minutes). CPU time was 2.3× wall time, so a CPU stage is likely the bottleneck rather than the GPU. Profile in Phase 1.
9. **Licensing.** RPM Fusion's FFmpeg is a GPL build. The RPM must depend on it, not bundle it, and keep OpenOSV's `LICENSE` and `NOTICE`.

## Phases

Each phase ends with something usable and a check done on the iMac with real footage.

### Phase 0: Foundation

- ~~Commit the two patches on a branch in the local OpenOSV clone, and push to `github.com/pcwizardsinc/DJIStudioCraft`.~~ Done: fork created, `djicraft` pushed (commit `2797546`).
- Keep `ROADMAP.md` and `BUILD-FEDORA.md` in the repo under `avata360/`.
- Make `djicraft` the fork's default branch.
- ~~Fresh-clone build on a second machine.~~ Done 2026-11-09 on a fresh Fedora 45 laptop with an NVIDIA GTX 1650 Ti (Secure Boot on); found and fixed two gaps in `BUILD-FEDORA.md` (`OpenCL-ICD-Loader-devel`, `miniz-devel`) and documented Secure Boot key enrollment.
- **Done when:** a fresh clone of DJIStudioCraft builds by following `BUILD-FEDORA.md` on a second machine (the NVIDIA laptop is the best choice: it doubles as the first NVIDIA test).

### Phase 1: Engine fixes

- VA-API hardware decoding (AMD, Intel): confirmed on the iMac's Radeon Pro 580X with `mesa-va-drivers-freeworld`. NVDEC confirmed on the NVIDIA laptop.
- Encoder auto-selection (`phase1-encoder.patch`): `--codec auto` probes for a working HEVC encoder, awaiting the iMac test.
- OpenCL render crash on the Radeon (Rusticl/libclc): open; render falls back to CPU meanwhile.
- The app sets `RUSTICL_ENABLE` itself: written (`phase1-zero-config.patch`), awaiting the iMac test. OpenCL confirmed on AMD and NVIDIA; Intel untested.
- Pick the D-Log M fit from the camera model: written (`phase1-zero-config.patch`), awaiting the iMac test. Normal-colour clips pass through untouched.
- Read the pilot's recorded view (pan, tilt, FOV, roll) and the gimbal mode (Follow / FPV) from the metadata, if the clip holds them.
- Find the render bottleneck (1.8 fps at 4K on the GTX 1650 Ti) and fix it; measure the iMac too.
- Full-resolution seam check on 4K-mode footage; test 8K footage once recorded.
- **Done when:** DangTree renders level, correctly coloured and GPU-accelerated with no manual flags, on the iMac and on an NVIDIA machine.

### Phase 2: Editor shell

- Project file (JSON): clips, timeline, per-clip settings, keyframes.
- Window layout modelled on DJI Studio (see `Interface/`): media bin, viewer, inspector, timeline.
- Open/save projects; import `.OSV` with its `.LRF` and `.SRT` found automatically, and Single Lens `.MP4` clips as flat video.
- **Done when:** a project with DangTree can be created, saved, closed and reopened.

### Phase 3: Live preview

- The engine renders inside the app rather than through `osvtool`.
- Playback from the `.LRF` proxy; the `.OSV` master only for stills and export.
- Drag in the viewer to pan and tilt, scroll to zoom; scrub the timeline.
- **Done when:** DangTree plays smoothly at preview size on the iMac and the view can be steered live.

### Phase 4: Keyframing

- Keyframes for pan, tilt, roll, field of view and view mode.
- Smooth interpolation between orientations (quaternion slerp; the engine already has it), with easing options.
- A new clip starts from the pilot's recorded flight view, as DJI's own playback does, ready to edit.
- **Done when:** a multi-keyframe camera move plays back smoothly and exports identically.

### Phase 5: View modes

- Asteroid (tiny planet), ultra-wide, wide, selfie and dewarp, plus flat and full 360.
- Follow (level) and FPV (rolls with the aircraft) framing.
- Animated transitions between modes.
- **Done when:** each mode renders correctly on DangTree and can be keyframed.

### Phase 6: Colour and stabilization panel

- Exposure, contrast, temperature, tint, shadows, highlights, saturation on top of the D-Log M conversion.
- Stabilization choices (off, horizon, full, smooth, smooth + horizon) and LUT import.
- **Done when:** a grade made in the panel matches the exported file.

### Phase 7: Audio

- Music lane under the video with trim, fade and volume; keep or mute the clip's own audio.
- **Done when:** DangTree exports with `No_Human_Hands.mp3` under it, in sync.

### Phase 8: Export

- MP4, H.264 and H.265, up to source resolution, with frame-rate choices (including 100 → 50 and slow motion).
- 360 output with spatial metadata so YouTube and players recognise it.
- DNxHR `.mov` for DaVinci Resolve; 16-bit PNG/TIFF stills for Photoshop.
- Noise-reduction option.
- **Done when:** exports open correctly in a player, YouTube's 360 checker and Resolve for Linux.

### Phase 9: AI subject tracking

- Draw a box around a subject; the tracker follows it across the 360 frame and writes pan/tilt keyframes.
- Hardest item. Model choice and licensing need research first: it must run on the Radeon or CPU and be shippable in the RPM.
- **Done when:** the trike in DangTree can be tracked through the clip from one box.

### Phase 10: RPM

- `.spec` file for package `avata360-editor`: builds from source, requires RPM Fusion's `ffmpeg-libs`, ships Dear ImGui as a bundled source tarball, installs the desktop entry and icon.
- Publish through a COPR repository so `dnf upgrade` delivers updates.
- **Done when:** a clean Fedora 45 install gets a working app from `dnf install` alone (plus RPM Fusion).

## Upstream contributions

Every engine fix is offered to [Kemerd/OpenOSV](https://github.com/Kemerd/OpenOSV) as a pull request, following its `CONTRIBUTING.md`: one topic per PR, `ctest` passing, a `CHANGELOG.md` entry under "Unreleased", ASCII console output, and evidence from real footage for any change to a verified convention ("add a test with the evidence, do not just flip a sign").

| PR | Content | State |
|---|---|---|
| Linux build | FFmpeg via pkg-config, tinyexr target names, Dear ImGui source build, install rules, desktop file | Ready to split out |
| Linux file dialogs | zenity / kdialog dialogs in OpenOSV Studio | Ready to split out |
| Portable CLI tests | `runShell` helper for POSIX shells | Ready to split out |
| Avata 360 levelling | Measured pitch correction; needs a unit test with synthetic attitudes, the `[DJI-Craft]` tag removed from its comment, and the before/after frames as evidence | Needs test |
| VA-API decoding | Phase 1 | Not started |
| Avata fit chosen automatically | Phase 1 | Not started |

## Open questions

- **Filters:** where do they appear: in DJI Fly's camera settings while recording (baked into the footage), or as a look applied in the app afterwards? This decides whether the editor reads them or recreates them.
- **The 4K / 100 fps mode** isn't on DJI's spec sheet (it lists 8K and 6K for 360° video). Which setting in DJI Fly produced the DangTree clip?
- **Pilot's view track:** confirm it's recorded in the `.OSV` metadata (check with `osvtool probe --raw` on a clip where the view was moved during flight).
- **Name and trademark:** "Avata" is DJI's trademark. Fine for personal use; worth considering before publishing the RPM or repo under that name.

## Log

| Date | Session |
|---|---|
| 2026-11-10 | iMac VA-API decode confirmed (`decoding with vaapi`). Two AMD-only bugs surfaced: the NVIDIA-only encoder default (fixed: `--codec auto` probe) and a Rusticl OpenCL kernel crash (open; CPU fallback works). |
| 2026-11-09 | NVIDIA footage test: 300 frames of DangTree at 3840×1920 rendered level with NVDEC + OpenCL + NVENC; levelling fix chose a −90° pitch turn (0.962 vs 0.193 for no turn); 1.8 fps steady state. |
| 2026-11-09 | Phase 0 complete: fresh clone built on an NVIDIA laptop (GTX 1650 Ti, OpenCL 3.0 via the proprietary driver). Build guide fixed for fresh installs and Secure Boot. |
| 2026-11-09 | Phase 0: GitHub fork `pcwizardsinc/DJIStudioCraft` created; branch `djicraft` pushed with the Fedora build and Avata levelling commits. |
| 2026-11-09 | Scope set: Avata 360 video only, all recording modes; any GPU on Fedora; name "Avata 360 Editor"; fixes go to the fork and upstream. |
| 2026-11-09 | Engine built and running on Fedora 45. Linux build patch (FFmpeg/tinyexr discovery, Dear ImGui, Linux file dialogs, install rules, portable CLI tests; 596/596 tests pass against FFmpeg 9.0.2). Avata 360 footage probed and rendered; levelling bug found and fixed. Decision: build everything into OpenOSV Studio as one application. |
