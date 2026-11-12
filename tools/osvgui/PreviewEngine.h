// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The OpenOSV / Avata 360 Editor Contributors
//
// PreviewEngine.h - in-process clip preview for Avata 360 Editor (Phase 3).
//
// The batch tool renders by shelling out to osvtool.  The editor's preview
// needs frames back as pixels, now, so PreviewEngine drives the engine
// (ImporterInstance) directly on a background thread: it opens the selected
// clip (its .LRF proxy when one sits beside the .OSV, for speed), renders one
// reframed frame on demand, and hands the UI thread an RGBA8 image to upload
// to a texture.
//
// Threading: open() and the first renderFrame() analyse the clip and take
// seconds, so all engine work runs on one worker thread.  The UI thread posts
// a Request (which clip, which frame, what framing) and later collects a
// Frame if one is ready; it never blocks.  Only the latest request matters, so
// a newer one supersedes any still queued - dragging the preview doesn't pile
// up work.
//
// This header pulls in no engine or ImGui types, so the UI can include it
// freely; the ImporterInstance lives behind a pimpl.

#ifndef OSVGUI_PREVIEWENGINE_H
#define OSVGUI_PREVIEWENGINE_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace osvgui {

/// What to render: a clip, a frame on its timeline, and the reframe camera.
/// Matching the inspector, angles are degrees and the view is rectilinear.
struct PreviewRequest {
    std::filesystem::path clip;    ///< The .OSV (or .LRF) to open.
    std::int64_t frame = 0;        ///< Timeline frame to show.
    int width = 960;               ///< Output width in pixels.
    int height = 540;              ///< Output height in pixels.
    double yawDeg = 0.0;           ///< Pan.
    double pitchDeg = 0.0;         ///< Tilt.
    double rollDeg = 0.0;          ///< Roll.
    double fovDeg = 90.0;          ///< Horizontal field of view.
    double exposureStops = 0.0;    ///< Exposure offset in stops (Adjust tab).
    bool equirect = false;         ///< true = full 360 equirect, ignore the camera.
    std::string colour = "709";    ///< Output transfer: "709", "hlg", "pq".
    std::string fit;               ///< D-Log M fit ("" = auto by camera).

    /// A monotonic id the caller bumps per request, echoed back on the Frame,
    /// so the UI can tell a fresh frame from a stale one.
    std::uint64_t id = 0;
};

/// A rendered frame: tightly-packed RGBA8, `width * height * 4` bytes.
struct PreviewFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;  ///< Row-major, 4 bytes per pixel.
    std::uint64_t id = 0;            ///< The request id this answers.
    std::string device;             ///< Renderer used ("cpu", "opencl", ...).
    std::int64_t frame = 0;          ///< Frame actually rendered.
    bool ok = false;                 ///< false => `error` says why.
    std::string error;
};

/// Per-clip facts the UI needs once a clip is open (frame count, fps, size),
/// published after the first successful open.
struct PreviewClipInfo {
    std::filesystem::path clip;
    std::int64_t frameCount = 0;
    double fps = 0.0;
    int equirectWidth = 0;
    int equirectHeight = 0;
    bool usingProxy = false;  ///< true when an .LRF was opened for the .OSV.
    bool valid = false;
};

class PreviewEngine {
public:
    PreviewEngine();
    ~PreviewEngine();

    PreviewEngine(const PreviewEngine&) = delete;
    PreviewEngine& operator=(const PreviewEngine&) = delete;

    /// Post a render request.  Returns at once; the worker renders the newest
    /// request and drops older pending ones.  A request for a different clip
    /// than the one open triggers an open() first.
    void request(const PreviewRequest& req) noexcept;

    /// Collect the most recent finished frame, if any, clearing it so the next
    /// call returns nothing until another render completes.  Non-blocking.
    [[nodiscard]] bool take(PreviewFrame& out) noexcept;

    /// The open clip's info, valid once the first open succeeded.  Non-blocking;
    /// returns false until then or after an open failure.
    [[nodiscard]] bool clipInfo(PreviewClipInfo& out) noexcept;

    /// True while the worker is opening or rendering, so the UI can show a
    /// spinner.  Non-blocking, a snapshot.
    [[nodiscard]] bool busy() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace osvgui

#endif  // OSVGUI_PREVIEWENGINE_H
