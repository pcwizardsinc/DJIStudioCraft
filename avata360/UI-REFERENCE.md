# Avata 360 Editor: UI reference

How DJI Studio for Windows works, as the model Avata 360 Editor follows. From a
Gemini writeup (2026-11), corrected and annotated against what we've verified.

> **Correction:** DJI's proxy files are `.LRF` only. The source writeup also
> mentions `.LIF`; that extension is not used by the Avata 360 and the DangTree
> clip confirmed `.LRF`. The editor matches `.LRF` and ignores `.LIF`.

## 1. Loading clips
- **Direct camera (USB-C).** Camera in File Transfer mode appears in the app; clips previewable without copying. *(Nice-to-have; Linux auto-mounts the camera as a drive, so file import covers it.)*
- **Local import.** `.OSV` beside its `.LRF` proxy in a folder; Create Project, drag into the media bin. *(Phase 2–3.)*
- **Proxy mode.** Edit against the low-res `.LRF` for smooth playback, render from the full `.OSV`. *(Phase 3 — this is exactly the preview plan.)*

## 2. Viewing
- **Interactive viewport.** Drag to pan the sphere, scroll to zoom. *(Phase 3.)*
- **Aspect modes.** Flat framed (16:9) vs full equirectangular. *(Phase 5 view modes.)*
- **Metadata panel.** Per-clip location, resolution, fps, codec, bit depth, duration. *(Phase 2 inspector — add a metadata read-out.)*

## 3. Timeline
- **Clip placement.** Drag clips from the bin onto a bottom timeline track.
- **Trim / split.** Drag edges to trim; Split (Ctrl+K) at the playhead; Cut Left / Cut Right.
- **Speed.** Playback-speed and slow-motion per clip.
> **Roadmap note:** this is a *multi-clip* timeline (several clips cut together on one track), broader than the per-clip keyframe track Phase 4 assumed. Captured as Phase 4b below.

## 4. 360 manipulation
- **Stitch + stabilize.** Horizon leveling, stabilization, optical stitch correction. Automatic selfie-stick erase. *(Engine has leveling/stab; selfie-stick erase = the guard stage — surface as a toggle, Phase 6.)*
- **Keyframe reframing.** Pan/tilt/roll/FOV keyframes with motion curves (Linear, Ease In, Ease Out). *(Phase 4.)*
- **AI subject tracking.** Draw a green box, Start Tracking, subject stays centered. *(Phase 9.)*
- **One-click views / presets.** Rotation, in-out zooms, asteroid, Selfie View (only if the in-camera Editing Assistant was on during recording). *(Phase 5; Selfie View depends on a metadata flag — ties to the Phase 1 "read the recorded flight view / camera flags" item.)*
- **Color grading.** D-Log M recovery, built-in Pro Filters, custom LUT import, manual exposure/contrast/saturation/white balance. *(Phase 6.)*

## 5. Export
- **Reframed flat video.** Keyframed 16:9 or vertical timeline to flat MP4. *(Phase 8.)*
- **Full 360 panoramic.** Stitched project to a spherical 360 file for VR / 360 platforms. *(Phase 8, with the 360 metadata already working.)*
