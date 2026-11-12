// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The OpenOSV / Avata 360 Editor Contributors
//
// PreviewEngine.cpp - the worker that drives ImporterInstance for the editor's
// preview (PreviewEngine.h).

#include "PreviewEngine.h"

#include "ImporterInstance.h"

#include "osv/color/ColorMath.h"
#include "osv/geom/VirtualCamera.h"
#include "osv/render/ImageRGBAf.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace osvgui {
namespace {

namespace pr = osv::premiere;
namespace fs = std::filesystem;

/// An OSV_TRANSFER_* id for the colour token, defaulting to Rec.709.
int transferId(const std::string& colour) {
    if (colour == "hlg") {
        return OSV_TRANSFER_HLG;
    }
    if (colour == "pq") {
        return OSV_TRANSFER_PQ;
    }
    return OSV_TRANSFER_REC709;
}

/// Convert a linear-ish float channel (already in the output transfer, 0..1) to
/// 8-bit, clamped.  The engine hands back display-referred values, so a plain
/// 0..1 scale is correct for an sRGB/Rec.709 preview surface.
inline std::uint8_t toByte(float v) {
    const float c = v <= 0.0f ? 0.0f : (v >= 1.0f ? 1.0f : v);
    return static_cast<std::uint8_t>(c * 255.0f + 0.5f);
}

/// The .LRF beside an .OSV, if one exists, so the preview plays the light
/// proxy.  Returns the input unchanged for a clip with no sibling proxy.
fs::path proxyFor(const fs::path& clip) {
    std::error_code ec;
    if (clip.extension() == ".OSV" || clip.extension() == ".osv") {
        fs::path lrf = clip;
        lrf.replace_extension(".LRF");
        if (fs::exists(lrf, ec)) {
            return lrf;
        }
        lrf.replace_extension(".lrf");
        if (fs::exists(lrf, ec)) {
            return lrf;
        }
    }
    return clip;
}

}  // namespace

struct PreviewEngine::Impl {
    std::thread worker;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop = false;

    // Latest request (only the newest matters).
    std::optional<PreviewRequest> pending;
    std::atomic<bool> working{false};

    // Latest finished frame, collected by take().
    std::optional<PreviewFrame> ready;

    // Clip info, published after the first open.
    std::optional<PreviewClipInfo> info;

    // Worker-owned: the open clip and which path it was opened from.
    std::unique_ptr<pr::ImporterInstance> clip;
    fs::path openPath;         // the source path the caller asked for
    fs::path openActual;       // what was actually opened (maybe the .LRF)

    Impl() { worker = std::thread([this] { run(); }); }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        cv.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
    }

    void run() {
        for (;;) {
            PreviewRequest req;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this] { return stop || pending.has_value(); });
                if (stop) {
                    return;
                }
                req = *pending;
                pending.reset();
                working.store(true);
            }

            PreviewFrame frame = render(req);

            {
                std::lock_guard<std::mutex> lock(mutex);
                ready = std::move(frame);
                working.store(false);
            }
        }
    }

    /// Open `req.clip` (its proxy when present) if it isn't already open.
    bool ensureOpen(const PreviewRequest& req, std::string& error) {
        if (clip && openPath == req.clip) {
            return true;
        }
        clip.reset();
        const fs::path actual = proxyFor(req.clip);
        auto instance = std::make_unique<pr::ImporterInstance>(actual);
        const osv::Status opened = instance->open();
        if (!opened.ok()) {
            error = "could not open " + actual.filename().string() + ": " + opened.error().toString();
            return false;
        }
        clip = std::move(instance);
        openPath = req.clip;
        openActual = actual;

        PreviewClipInfo ci;
        ci.clip = req.clip;
        ci.frameCount = static_cast<std::int64_t>(clip->timelineFrameCount());
        ci.fps = clip->fps();
        const pr::OutputGeometry native = clip->geometryFor(clip->prefs());
        ci.equirectWidth = native.width;
        ci.equirectHeight = native.height;
        ci.usingProxy = (actual != req.clip);
        ci.valid = true;
        info = ci;
        return true;
    }

    PreviewFrame render(const PreviewRequest& req) {
        PreviewFrame out;
        out.id = req.id;
        out.frame = req.frame;

        std::string error;
        if (!ensureOpen(req, error)) {
            out.error = error;
            return out;
        }

        // Hard ceiling for a PREVIEW.  The engine renders into float RGBA (16
        // bytes a pixel) and keeps a frame cache, so an unbounded preview size
        // is the difference between 8 MB and most of a laptop's RAM; a request
        // for the clip's native equirect has had the editor OOM-killed.  The
        // editor already asks small, this is the backstop.
        constexpr int kMaxPreviewPixels = 1280 * 720;
        int w = std::clamp(req.width, 16, 1920);
        int h = std::clamp(req.height, 16, 1080);
        if (static_cast<long long>(w) * h > kMaxPreviewPixels) {
            const double scale = std::sqrt(static_cast<double>(kMaxPreviewPixels) /
                                           (static_cast<double>(w) * static_cast<double>(h)));
            w = std::max(16, static_cast<int>(w * scale));
            h = std::max(16, static_cast<int>(h * scale));
            if (req.equirect) {
                h = std::max(8, w / 2);  // equirect must stay 2:1
            }
        }

        std::lock_guard<std::mutex> clipLock(clip->lock());

        pr::PrefsBlob settings = clip->prefs();
        settings.exposureStops = static_cast<float>(
            std::clamp(req.exposureStops, -4.0, 4.0));
        clip->applyPrefs(&settings, pr::PrefsBlob::kSize);

        pr::OutputGeometry geometry;
        geometry.width = w;
        geometry.height = h;
        if (!req.equirect) {
            osv::geom::VirtualCamera cam;
            cam.projection = osv::geom::Projection::Rectilinear;
            cam.w = w;
            cam.h = h;
            cam.yawDeg = req.yawDeg;
            cam.pitchDeg = req.pitchDeg;
            cam.rollDeg = req.rollDeg;
            cam.hfovDeg = std::clamp(req.fovDeg, 10.0, 170.0);
            geometry.view = cam;
        }
        if (!geometry.valid()) {
            out.error = "no usable preview size for this clip";
            return out;
        }

        const std::uint32_t total = clip->timelineFrameCount();
        std::uint32_t f = 0;
        if (total > 0) {
            const std::int64_t clamped = std::clamp<std::int64_t>(req.frame, 0, static_cast<std::int64_t>(total) - 1);
            f = static_cast<std::uint32_t>(clamped);
        }

        // Draft render: the preview wants a frame promptly, not the final
        // analysed pass.  renderFrame returns a pointer into the engine's cache
        // valid only under this lock, so copy it out before releasing.
        auto rendered = clip->renderFrame(f, geometry, /*draft=*/true, pr::RenderPurpose::Interactive,
                                          transferId(req.colour));
        if (!rendered.ok()) {
            out.error = rendered.error().toString();
            return out;
        }
        const osv::render::ImageRGBAf* img = rendered.value();
        if (!img || img->w == 0 || img->h == 0) {
            out.error = "the preview frame was empty";
            return out;
        }

        out.width = static_cast<int>(img->w);
        out.height = static_cast<int>(img->h);
        out.rgba.resize(static_cast<std::size_t>(img->w) * img->h * 4u);
        const float* src = img->data.data();
        std::uint8_t* dst = out.rgba.data();
        const std::size_t pixels = static_cast<std::size_t>(img->w) * img->h;
        for (std::size_t i = 0; i < pixels; ++i) {
            dst[i * 4 + 0] = toByte(src[i * 4 + 0]);
            dst[i * 4 + 1] = toByte(src[i * 4 + 1]);
            dst[i * 4 + 2] = toByte(src[i * 4 + 2]);
            dst[i * 4 + 3] = 255;  // opaque; the engine's alpha is coverage, not transparency here
        }
        out.device = clip->rendererName();
        out.ok = true;
        return out;
    }
};

PreviewEngine::PreviewEngine() : m_impl(std::make_unique<Impl>()) {}
PreviewEngine::~PreviewEngine() = default;

void PreviewEngine::request(const PreviewRequest& req) noexcept {
    try {
        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            m_impl->pending = req;  // supersede any older pending request
        }
        m_impl->cv.notify_all();
    } catch (...) {
        // A failed notify just means the worker picks it up on the next wake.
    }
}

bool PreviewEngine::take(PreviewFrame& out) noexcept {
    try {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (!m_impl->ready.has_value()) {
            return false;
        }
        out = std::move(*m_impl->ready);
        m_impl->ready.reset();
        return true;
    } catch (...) {
        return false;
    }
}

bool PreviewEngine::clipInfo(PreviewClipInfo& out) noexcept {
    try {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (!m_impl->info.has_value() || !m_impl->info->valid) {
            return false;
        }
        out = *m_impl->info;
        return true;
    } catch (...) {
        return false;
    }
}

bool PreviewEngine::busy() const noexcept {
    return m_impl->working.load();
}

}  // namespace osvgui
