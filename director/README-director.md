# 360 Director

Direct camera moves inside your 360 footage, then render them as a normal
flat video. Drag the picture while it plays and whatever you look at is
what gets recorded. One-click moves add a look behind, a look up, a look
down or a full 360 spin.

Built for DJI Avata 360 clips stitched by OpenOSV on Fedora, but it works
with any equirectangular (2:1) 360 video.

## Install

360 Director ships inside the `openosv` package (release 0.8 and later),
alongside osvtool and OpenOSV Studio. Open **360 Director** from Activities,
or right-click **OpenOSV Studio** and pick **360 Director**. It opens in
your web browser and works offline. `director-proxy` and `director-render`
are in `/usr/bin`.

## Use it

1. **Make a 360 master** with OpenOSV (full sphere, normal color). Render
   only the stretch you need; stitching is the slow part.

   ```bash
   osvtool render "MyClip.OSV" --mode equirect --size 3840x1920 --fit avata360 --color 709 --range 600-4200 --out MyClip.mp4
   ```

2. **Make a preview copy** for the browser (a minute or two):

   ```bash
   director-proxy MyClip.mp4
   ```

3. **Direct.** Open 360 Director, click **Open video**, pick
   `MyClip.proxy.mp4`.
   - Drag to look around, scroll to zoom.
   - **Record** (R): it plays, and your drags become the camera move.
     Choose ½× speed for smoother moves.
   - **Add keyframe** (K): pins the current view at the playhead. The
     camera glides smoothly between keyframes.
   - **Moves**: Look behind, Look up, Look down, Full 360 spin, Resume
     forward. Each starts at the playhead from whatever you're looking at.
     **Resume forward** glides back to the level view straight ahead (it
     keeps your zoom). Use it to end a Look behind whenever you choose,
     with "Come back to the starting view" unticked.
   - **Set start / Set end** (I / O) choose what gets rendered.
   - Undo with Ctrl+Z. Your work is saved automatically in the browser.

4. **Render.** Click **Render video**, type the original .OSV name and the
   frame your 360 render began at (its `--range` start, or 0), then
   **Save path file**. In a terminal, go to your footage folder and run one
   of the two commands shown:

   - **Straight from the .OSV** (needs OpenOSV release 0.7 or later). One
     pass that stitches only the frame you see:

     ```bash
     osvtool render DJI_20260815142440_0001_D.OSV --path ~/Downloads/MyClip.path.json --fit avata360 --color 709 --out MyClip.director.mp4
     ```

   - **From the 360 master** you already rendered. Faster, because the
     stitching is already done:

     ```bash
     director-render ~/Downloads/MyClip.path.json
     ```

   Both include the clip's audio. Use the same OpenOSV settings (such as
   `--stab`) for the .OSV render as for the 360 master you directed on, so
   the horizon and heading line up.

## Keys

| Key | Does |
| --- | --- |
| Space | Play or pause (also stops recording) |
| R | Record moves |
| K | Add keyframe |
| Delete | Delete the selected keyframe |
| I / O | Set start / end |
| ← / → | Step one frame; with Shift, one second |
| Ctrl+Z | Undo |

## Good to know

- **Zoom (FOV)** is the field of view across the frame's long side, so a
  9:16 vertical frame looks the same as 16:9 turned on its side.
- The preview and the render use the same camera maths, checked against
  ffmpeg's own 360 converter.
- `director-render --help` lists overrides: `--size`, `--crf`,
  `--codec libx264`, `--interp cubic`, `--master`, `--out`.
- Recording replaces earlier moves in the stretch you record over.
