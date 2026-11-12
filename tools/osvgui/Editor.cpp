// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The OpenOSV / Avata 360 Editor Contributors
//
// Editor.cpp - the four-region editor layout for Avata 360 Editor (Phase 2).
//
// Drawn by App::drawEditor() in place of the batch queue when editor mode is
// on.  The regions follow DJI Studio: a media bin down the left, the preview
// filling the centre, an inspector on the right for the selected clip, and a
// timeline strip across the bottom.  This step lays out the regions and fills
// the media bin and inspector from the open Project (Project.h); the preview
// shows a placeholder until Phase 3 renders real frames into it, and the
// timeline is a static ruler until Phase 4 adds keyframes.
//
// The batch-queue UI (Ui.cpp) is untouched: the two layouts share the App
// state but never draw at once.

#include "Project.h"
#include "Style.h"
#include "Ui.h"
#include "Widgets.h"

#include <imgui.h>

#include <GLFW/glfw3.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace osvgui {

using ui::px;

namespace {

/// Bold text at a given size and colour (Editor-local; Ui.cpp keeps its own).
void boldText(const char* text, float size, const ImVec4& colour) {
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::PushFont(nullptr, size);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

/// Small wrapped secondary text.
void smallText(const char* text, const ImVec4& colour) {
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::PushFont(nullptr, style::kSmallSize);
    ImGui::TextWrapped("%s", text);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

/// A panel heading in the editor's secondary text colour.
void panelTitle(const char* text) {
    const style::Palette& p = style::palette();
    boldText(text, style::kSmallSize, p.text2);
    ImGui::Dummy(ImVec2(0.0f, px(4.0f)));
}

}  // namespace

// ---------------------------------------------------------------------------
//  Keep the project's clip list in step with the queue.
// ---------------------------------------------------------------------------
void App::syncProjectFromQueue() {
    // Build a project whose clips mirror the queue, preserving any edits the
    // user already made to clips that are still present (matched by path).
    std::vector<std::filesystem::path> sources;
    sources.reserve(m_queue.items().size());
    for (const ClipItem& item : m_queue.items()) {
        sources.push_back(item.path);
    }

    Project next = projectFromClips(sources, m_settings);
    // Carry over edits from the previous project for clips that remain.
    for (ProjectClip& clip : next.clips) {
        for (const ProjectClip& old : m_project.clips) {
            if (old.source == clip.source) {
                clip.edit = old.edit;
                clip.name = old.name;
                clip.inFrame = old.inFrame;
                clip.outFrame = old.outFrame;
                clip.keyframesJson = old.keyframesJson;
                break;
            }
        }
    }
    next.output = m_settings;
    next.path = m_project.path;
    m_project = std::move(next);
    if (m_editClip >= m_project.clips.size()) {
        m_editClip = m_project.empty() ? 0 : m_project.clips.size() - 1;
    }
}

// ---------------------------------------------------------------------------
//  The mode toggle: Batch <-> Editor.
// ---------------------------------------------------------------------------
void App::drawModeToggle(float x, float y, float width) {
    const style::Palette& p = style::palette();
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginGroup();

    const float segW = std::min(px(220.0f), width);
    const float h = px(30.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + segW, origin.y + h), style::u32(p.field), h * 0.5f);

    const char* labels[2] = {"Batch", "Editor"};
    const float half = segW * 0.5f;
    for (int i = 0; i < 2; ++i) {
        const bool active = (i == 1) == m_editorMode;
        const ImVec2 a(origin.x + half * static_cast<float>(i), origin.y);
        if (active) {
            dl->AddRectFilled(ImVec2(a.x + px(2.0f), a.y + px(2.0f)),
                              ImVec2(a.x + half - px(2.0f), a.y + h - px(2.0f)), style::u32(p.accent), h * 0.5f);
        }
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton(labels[i], ImVec2(half, h))) {
            const bool wantEditor = (i == 1);
            if (wantEditor != m_editorMode) {
                m_editorMode = wantEditor;
                if (m_editorMode) {
                    syncProjectFromQueue();
                }
            }
        }
        const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(a.x + (half - ts.x) * 0.5f, a.y + (h - ts.y) * 0.5f),
                    style::u32(active ? p.onAccent : p.text2), labels[i]);
    }
    ImGui::EndGroup();
}

// ---------------------------------------------------------------------------
//  Media bin: the project's clips as a vertical list.
// ---------------------------------------------------------------------------
void App::drawMediaBin(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##mediaBin", size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_None);
    panelTitle("MEDIA");

    if (m_project.empty()) {
        smallText("No clips yet.\nAdd .OSV files with the + button\nor drag them in.", p.text3);
        ImGui::EndChild();
        return;
    }

    for (std::size_t i = 0; i < m_project.clips.size(); ++i) {
        const ProjectClip& clip = m_project.clips[i];
        ImGui::PushID(static_cast<int>(i));
        const bool selected = (i == m_editClip);
        if (ImGui::Selectable("##clipRow", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, px(40.0f)))) {
            m_editClip = i;
        }
        // Draw the name and folder over the selectable.
        const ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(ImVec2(min.x + px(6.0f), min.y + px(5.0f)), style::u32(p.text), clip.name.c_str());
        const std::string folder = pathToUtf8(clip.source.parent_path().filename());
        dl->AddText(ImVec2(min.x + px(6.0f), min.y + px(21.0f)), style::u32(p.text3), folder.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
//  Preview: the selected clip's frame, rendered in-process.
// ---------------------------------------------------------------------------
void App::uploadPreviewFrame(const PreviewFrame& frame) {
    if (!frame.ok || frame.width <= 0 || frame.height <= 0) {
        return;
    }
    if (m_previewTexture == 0) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        m_previewTexture = tex;
    }
    glBindTexture(GL_TEXTURE_2D, m_previewTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (frame.width == m_previewTexW && frame.height == m_previewTexH) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, GL_RGBA, GL_UNSIGNED_BYTE,
                        frame.rgba.data());
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     frame.rgba.data());
        m_previewTexW = frame.width;
        m_previewTexH = frame.height;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    m_previewShownId = frame.id;
    m_previewDevice = frame.device;
}

void App::updatePreview(int paneW, int paneH) {
    // Collect any finished frame first (GL upload must run on this thread).
    PreviewFrame frame;
    while (m_preview.take(frame)) {
        if (frame.ok) {
            uploadPreviewFrame(frame);
            m_previewError.clear();
        } else {
            m_previewError = frame.error;
        }
    }

    if (m_project.empty() || m_editClip >= m_project.clips.size()) {
        return;
    }
    const ProjectClip& clip = m_project.clips[m_editClip];
    // Preview size: small and bounded.  A preview must never ask for the
    // clip's native equirect (3840x1920 here, 8K on a 8K clip): at 4 floats a
    // pixel that is hundreds of MB per frame and the engine's cache holds
    // more than one, which is enough to have the app killed on a 16 GB
    // machine.  Cap at 960 wide, and for equirect keep the 2:1 the projection
    // requires (a non-2:1 equirect geometry is rejected as invalid).
    const bool wantEquirect = (clip.edit.mode == "equirect");
    int w = std::clamp(paneW, 160, 960);
    int h = std::clamp(paneH, 90, 540);
    if (wantEquirect) {
        w = std::min(w, 960);
        h = w / 2;
    }

    const double expStops = clip.edit.gradeEnabled ? clip.edit.exposure * 3.0 : 0.0;
    const bool changed = clip.source != m_previewClip || m_playhead != m_previewFrame ||
                         clip.edit.yaw != m_previewYaw || clip.edit.pitch != m_previewPitch ||
                         clip.edit.fov != m_previewFov || expStops != m_previewExposure ||
                         w != m_previewW || h != m_previewH;
    if (!changed) {
        return;
    }
    m_previewClip = clip.source;
    m_previewFrame = m_playhead;
    m_previewYaw = clip.edit.yaw;
    m_previewPitch = clip.edit.pitch;
    m_previewFov = clip.edit.fov;
    m_previewExposure = expStops;
    m_previewW = w;
    m_previewH = h;

    PreviewRequest req;
    req.clip = clip.source;
    req.frame = m_playhead;
    req.width = w;
    req.height = h;
    req.yawDeg = clip.edit.yaw;
    req.pitchDeg = clip.edit.pitch;
    req.rollDeg = clip.edit.roll;
    req.fovDeg = clip.edit.fov;
    req.exposureStops = clip.edit.gradeEnabled ? clip.edit.exposure * 3.0 : 0.0;
    req.equirect = wantEquirect;
    req.colour = "709";
    req.id = ++m_previewReqId;
    m_preview.request(req);
}

void App::drawPreview(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##preview", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Keep the preview request in step with the pane and the selected clip.
    updatePreview(static_cast<int>(avail.x), static_cast<int>(avail.y));

    // The frame fits inside the pane at 16:9 (or the texture's own ratio).
    float boxW = avail.x;
    float boxH = avail.y;
    if (m_previewTexW > 0 && m_previewTexH > 0) {
        const float ar = static_cast<float>(m_previewTexW) / static_cast<float>(m_previewTexH);
        if (boxW / boxH > ar) {
            boxW = boxH * ar;
        } else {
            boxH = boxW / ar;
        }
    }
    const ImVec2 bmin(origin.x + (avail.x - boxW) * 0.5f, origin.y + (avail.y - boxH) * 0.5f);
    const ImVec2 bmax(bmin.x + boxW, bmin.y + boxH);

    if (m_previewTexture != 0 && m_previewTexW > 0) {
        dl->AddImage(ImTextureRef(static_cast<ImTextureID>(static_cast<std::uintptr_t>(m_previewTexture))), bmin, bmax);
        dl->AddRect(bmin, bmax, style::u32(p.border), px(4.0f));
    } else {
        dl->AddRectFilled(bmin, bmax, style::u32(ImVec4(0.04f, 0.04f, 0.05f, 1.0f)), px(6.0f));
        dl->AddRect(bmin, bmax, style::u32(p.border), px(6.0f));
    }

    // Drag inside the preview to pan/tilt the selected clip.
    ImGui::SetCursorScreenPos(bmin);
    ImGui::InvisibleButton("##previewDrag", ImVec2(std::max(1.0f, boxW), std::max(1.0f, boxH)));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !m_project.empty() &&
        m_editClip < m_project.clips.size()) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        ProjectClip& clip = m_project.clips[m_editClip];
        // A drag across the pane maps to the clip's field of view, so the image
        // tracks the cursor roughly 1:1.
        const double degPerPxX = boxW > 1.0f ? clip.edit.fov / boxW : 0.0;
        const double degPerPxY = degPerPxX;  // square pixels
        clip.edit.yaw = std::clamp(clip.edit.yaw - d.x * degPerPxX, -180.0, 180.0);
        clip.edit.pitch = std::clamp(clip.edit.pitch + d.y * degPerPxY, -90.0, 90.0);
        m_project.dirty = true;
        m_previewDragging = true;
    } else {
        m_previewDragging = false;
    }
    // Scroll to zoom (field of view).
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f && !m_project.empty() &&
        m_editClip < m_project.clips.size()) {
        ProjectClip& clip = m_project.clips[m_editClip];
        clip.edit.fov = std::clamp(clip.edit.fov - ImGui::GetIO().MouseWheel * 4.0, 10.0, 170.0);
        m_project.dirty = true;
    }

    // A small status line: which renderer, or an error.
    const char* status = nullptr;
    std::string buf;
    if (!m_previewError.empty()) {
        status = m_previewError.c_str();
    } else if (m_project.empty()) {
        status = "Add a clip to preview it";
    } else if (m_previewTexture == 0) {
        status = m_preview.busy() ? "Rendering..." : "Preparing preview...";
    } else if (!m_previewDevice.empty()) {
        buf = m_previewDevice + (m_preview.busy() ? "  (rendering)" : "");
        status = buf.c_str();
    }
    if (status) {
        dl->AddText(ImVec2(origin.x + px(8.0f), origin.y + avail.y - px(18.0f)), style::u32(p.text3), status);
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
//  Inspector: the selected clip's edit settings.
// ---------------------------------------------------------------------------
namespace {

/// A DJI-style labelled slider: label on its own line, value shown, full width.
/// Returns true and writes `value` when the user moves it.  `fmt` is a printf
/// format for the number (e.g. "%.0f°").  Marks the project dirty via `dirty`.
bool gradeSlider(const char* label, double& value, double lo, double hi, const char* fmt, bool& dirty) {
    const style::Palette& p = style::palette();
    ImGui::PushID(label);
    ImGui::TextColored(p.text2, "%s", label);
    ImGui::SameLine();
    char buf[32];
    std::snprintf(buf, sizeof buf, fmt, value);
    const ImVec2 ts = ImGui::CalcTextSize(buf);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ts.x);
    ImGui::TextColored(p.text3, "%s", buf);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    float v = static_cast<float>(value);
    bool changed = false;
    if (ImGui::SliderFloat("##s", &v, static_cast<float>(lo), static_cast<float>(hi), "", ImGuiSliderFlags_NoInput)) {
        value = v;
        dirty = true;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

/// A toggle row: label left, checkbox right.
bool toggleRow(const char* label, bool& value, bool& dirty) {
    const style::Palette& p = style::palette();
    ImGui::PushID(label);
    ImGui::TextColored(p.text2, "%s", label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - px(24.0f));
    bool changed = false;
    if (ImGui::Checkbox("##t", &value)) {
        dirty = true;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

/// A small heading inside a tab.
void tabSection(const char* text) {
    const style::Palette& p = style::palette();
    ImGui::Dummy(ImVec2(0.0f, px(6.0f)));
    boldText(text, style::kSmallSize, p.text);
    ImGui::Dummy(ImVec2(0.0f, px(2.0f)));
}

}  // namespace

void App::drawInspector(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##inspector", size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_None);

    if (m_project.empty() || m_editClip >= m_project.clips.size()) {
        panelTitle("INSPECTOR");
        smallText("Select a clip to edit its framing, colour and stabilisation.", p.text3);
        ImGui::EndChild();
        return;
    }

    ProjectClip& clip = m_project.clips[m_editClip];
    GuiSettings& e = clip.edit;
    bool& dirty = m_project.dirty;
    boldText(clip.name.c_str(), style::kBodySize, p.text);
    ImGui::Dummy(ImVec2(0.0f, px(4.0f)));

    if (ImGui::BeginTabBar("##inspectorTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        // ---- Video --------------------------------------------------------------------
        if (ImGui::BeginTabItem("Video")) {
            tabSection("Select Perspective");
            // Perspective maps to stabilisation: Camera View replays the flight
            // view (smooth+horizon), Free View is free reframe (horizon), and
            // Direction Lock holds the camera direction (full).
            const char* persp[] = {"Camera View", "Free View", "Direction Lock"};
            const char* perspTok[] = {"smooth-horizon", "horizon", "full"};
            int cur = 0;
            for (int i = 0; i < 3; ++i) {
                if (e.stab == perspTok[i]) {
                    cur = i;
                }
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##persp", &cur, persp, 3)) {
                e.stab = perspTok[cur];
                dirty = true;
            }

            tabSection("Stabilization");
            bool horizon = (e.stab == "horizon" || e.stab == "smooth-horizon");
            if (toggleRow("Horizon Leveling", horizon, dirty)) {
                e.stab = horizon ? "smooth-horizon" : "smooth";
            }

            tabSection("Manual Framing");
            const char* modes[] = {"Wide", "Ultra wide", "Dewarp", "Asteroid", "Crystal ball"};
            const char* modeTok[] = {"wide", "ultra-wide", "dewarping", "asteroid", "crystal-ball"};
            int vcur = 0;
            for (int i = 0; i < 5; ++i) {
                if (e.view == modeTok[i]) {
                    vcur = i;
                }
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##view", &vcur, modes, 5)) {
                e.view = modeTok[vcur];
                e.mode = "reframe";
                dirty = true;
            }

            tabSection("Framing");
            gradeSlider("FOV", e.fov, 10.0, 170.0, "%.0f\xc2\xb0", dirty);
            gradeSlider("Pan", e.yaw, -180.0, 180.0, "%.0f\xc2\xb0", dirty);
            gradeSlider("Tilt", e.pitch, -90.0, 90.0, "%.0f\xc2\xb0", dirty);
            gradeSlider("Roll", e.roll, -180.0, 180.0, "%.0f\xc2\xb0", dirty);

            ImGui::Dummy(ImVec2(0.0f, px(8.0f)));
            ImGui::BeginDisabled(true);
            ImGui::Button("Intelligent Tracking", ImVec2(ImGui::GetContentRegionAvail().x, px(30.0f)));
            ImGui::EndDisabled();
            smallText("Tracking and keyframe animation arrive in a later step.", p.text3);
            ImGui::EndTabItem();
        }

        // ---- Image --------------------------------------------------------------------
        if (ImGui::BeginTabItem("Image")) {
            tabSection("Flight Mode");
            toggleRow("FPV Mode", e.fpvMode, dirty);
            bool follow = !e.fpvMode;
            if (toggleRow("Follow Mode", follow, dirty)) {
                e.fpvMode = !follow;
            }
            tabSection("Chromatic Aberration");
            toggleRow("Correction", e.chromaticAberration, dirty);
            tabSection("Stitching Optimization");
            toggleRow("Optical Flow Stitching", e.opticalFlowStitch, dirty);
            tabSection("Distortion Optimization");
            toggleRow("Dewarp", e.dewarp, dirty);
            tabSection("Sun / Selfie-stick");
            toggleRow("Erase", e.flare, dirty);
            ImGui::EndTabItem();
        }

        // ---- Adjust -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Adjust")) {
            toggleRow("Color Recovery (D-Log M)", e.gradeEnabled, dirty);
            ImGui::BeginDisabled(!e.gradeEnabled);
            tabSection("Light");
            gradeSlider("Exposure", e.exposure, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Luminance", e.luminance, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Contrast", e.contrast, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Highlights", e.highlights, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Shadows", e.shadows, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Whites", e.whites, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Blacks", e.blacks, -1.0, 1.0, "%.2f", dirty);
            tabSection("Color");
            gradeSlider("Temperature", e.temperature, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Tint", e.tint, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Saturation", e.saturation, -1.0, 1.0, "%.2f", dirty);
            gradeSlider("Vibrance", e.vibrance, -1.0, 1.0, "%.2f", dirty);
            ImGui::EndDisabled();
            ImGui::Dummy(ImVec2(0.0f, px(6.0f)));
            smallText("Exposure previews live and exports; the other grade controls are stored per clip and apply where the engine supports them.", p.text3);
            ImGui::EndTabItem();
        }

        // ---- Speed --------------------------------------------------------------------
        if (ImGui::BeginTabItem("Speed")) {
            tabSection("Playback speed");
            gradeSlider("Speed", e.speed, 0.25, 4.0, "%.2fx", dirty);
            smallText("The 100 fps source conforms to 50 fps; slower speeds give slow motion.", p.text3);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
//  Timeline: a static ruler until Phase 4.
// ---------------------------------------------------------------------------
void App::drawTimeline(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##timeline", size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_None);
    panelTitle("TIMELINE");

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float trackY = origin.y + px(6.0f);
    const float trackH = std::max(px(10.0f), avail.y - px(16.0f));
    dl->AddRectFilled(ImVec2(origin.x, trackY), ImVec2(origin.x + avail.x, trackY + trackH),
                      style::u32(p.field), px(4.0f));

    // Tick marks every tenth of the width, as a placeholder ruler.
    for (int i = 0; i <= 10; ++i) {
        const float tx = origin.x + avail.x * (static_cast<float>(i) / 10.0f);
        dl->AddLine(ImVec2(tx, trackY), ImVec2(tx, trackY + trackH), style::u32(p.border));
    }
    const char* msg = "Scrubbing and keyframes arrive in a later step.";
    dl->AddText(ImVec2(origin.x + px(6.0f), trackY + trackH * 0.5f - px(6.0f)), style::u32(p.text3), msg);
    ImGui::EndChild();
}

/// Transport controls under the preview: frame step, play (disabled until
/// playback lands) and the current timecode, as DJI Studio shows.
void App::drawTransport(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##transport", size, ImGuiChildFlags_None, ImGuiWindowFlags_None);
    PreviewClipInfo info;
    const bool haveInfo = m_preview.clipInfo(info);
    const double fps = (haveInfo && info.fps > 0.0) ? info.fps : 30.0;
    const std::int64_t total = haveInfo ? info.frameCount : 0;

    const auto timecode = [fps](std::int64_t frame) {
        const double sec = static_cast<double>(frame) / (fps > 0.0 ? fps : 30.0);
        const int s = static_cast<int>(sec);
        char buf[32];
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d.%02d", s / 3600, (s / 60) % 60, s % 60,
                      static_cast<int>((sec - s) * 100.0));
        return std::string(buf);
    };

    const float bw = px(34.0f);
    if (ImGui::Button("|<", ImVec2(bw, px(26.0f)))) {
        m_playhead = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("<", ImVec2(bw, px(26.0f)))) {
        m_playhead = std::max<std::int64_t>(0, m_playhead - 1);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(true);
    ImGui::Button(">", ImVec2(bw, px(26.0f)));
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(bw, px(26.0f)))) {
        if (total > 0) {
            m_playhead = std::min<std::int64_t>(total - 1, m_playhead + 1);
        }
    }
    ImGui::SameLine();
    const std::string tc = timecode(m_playhead) + "  /  " + timecode(total > 0 ? total - 1 : 0);
    ImGui::TextColored(p.text2, "%s", tc.c_str());
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
//  Tool rail: the editor's left-hand tool tabs (File, Music, Filter, ...).
//  Only File is implemented; the rest are placeholders that say so, so the
//  shape of the app matches DJI Studio while the features are built.
// ---------------------------------------------------------------------------
void App::drawToolRail(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##toolRail", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
    static const char* kTools[] = {"File", "Music", "Filter", "Effects", "Camera", "Transit.", "Text"};
    const float w = ImGui::GetContentRegionAvail().x;
    for (int i = 0; i < 7; ++i) {
        const bool active = (i == m_editorTool);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? p.accent : ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? p.onAccent : p.text2);
        if (ImGui::Button(kTools[i], ImVec2(w, px(34.0f)))) {
            m_editorTool = i;
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
//  Lay the regions out.
// ---------------------------------------------------------------------------
void App::drawEditor(float x, float y, float width, float height) {
    const float gap = px(8.0f);
    const float railW = px(78.0f);
    const float binW = std::clamp(width * 0.16f, px(170.0f), px(260.0f));
    const float inspW = std::clamp(width * 0.24f, px(260.0f), px(380.0f));
    const float timelineH = std::clamp(height * 0.24f, px(120.0f), px(220.0f));
    const float transportH = px(34.0f);
    const float topH = std::max(px(160.0f), height - timelineH - gap);
    const float centreW =
        std::max(px(200.0f), width - railW - binW - inspW - 3.0f * gap);

    float cx = x;
    ImGui::SetCursorPos(ImVec2(cx, y));
    drawToolRail(ImVec2(railW, topH));
    cx += railW + gap;

    ImGui::SetCursorPos(ImVec2(cx, y));
    drawMediaBin(ImVec2(binW, topH));
    cx += binW + gap;

    ImGui::SetCursorPos(ImVec2(cx, y));
    drawPreview(ImVec2(centreW, topH - transportH));
    ImGui::SetCursorPos(ImVec2(cx, y + topH - transportH + px(2.0f)));
    drawTransport(ImVec2(centreW, transportH - px(2.0f)));
    cx += centreW + gap;

    ImGui::SetCursorPos(ImVec2(cx, y));
    drawInspector(ImVec2(inspW, topH));

    ImGui::SetCursorPos(ImVec2(x, y + topH + gap));
    drawTimeline(ImVec2(width, timelineH));
}

}  // namespace osvgui
