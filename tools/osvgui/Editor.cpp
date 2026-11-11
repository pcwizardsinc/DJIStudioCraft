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

#include <algorithm>
#include <cstdint>
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
//  Preview: a placeholder until Phase 3 renders into it.
// ---------------------------------------------------------------------------
void App::drawPreview(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##preview", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // A 2:1 equirectangular preview box centred in the region, black like a
    // video surface, with a label.  Phase 3 draws the decoded frame here.
    const float boxW = std::min(avail.x, avail.y * 2.0f);
    const float boxH = boxW * 0.5f;
    const ImVec2 bmin(origin.x + (avail.x - boxW) * 0.5f, origin.y + (avail.y - boxH) * 0.5f);
    const ImVec2 bmax(bmin.x + boxW, bmin.y + boxH);
    dl->AddRectFilled(bmin, bmax, style::u32(ImVec4(0.04f, 0.04f, 0.05f, 1.0f)), px(6.0f));
    dl->AddRect(bmin, bmax, style::u32(p.border), px(6.0f));

    const char* msg = m_project.empty() ? "Add a clip to preview it" : "Preview (coming in the next step)";
    const ImVec2 ts = ImGui::CalcTextSize(msg);
    dl->AddText(ImVec2((bmin.x + bmax.x - ts.x) * 0.5f, (bmin.y + bmax.y - ts.y) * 0.5f), style::u32(p.text3), msg);
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
//  Inspector: the selected clip's edit settings.
// ---------------------------------------------------------------------------
void App::drawInspector(ImVec2 size) {
    const style::Palette& p = style::palette();
    ImGui::BeginChild("##inspector", size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_None);
    panelTitle("INSPECTOR");

    if (m_project.empty() || m_editClip >= m_project.clips.size()) {
        smallText("Select a clip to edit its framing, colour and stabilisation.", p.text3);
        ImGui::EndChild();
        return;
    }

    ProjectClip& clip = m_project.clips[m_editClip];
    boldText(clip.name.c_str(), style::kBodySize, p.text);
    ImGui::Dummy(ImVec2(0.0f, px(8.0f)));

    // Framing controls, bound to this clip's edit state.  Editing marks the
    // project dirty so a close prompt and the save path know there is work.
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(w);
    float yaw = static_cast<float>(clip.edit.yaw);
    if (ImGui::SliderFloat("##yaw", &yaw, -180.0f, 180.0f, "Pan %.0f°")) {
        clip.edit.yaw = yaw;
        m_project.dirty = true;
    }
    ImGui::SetNextItemWidth(w);
    float pitch = static_cast<float>(clip.edit.pitch);
    if (ImGui::SliderFloat("##pitch", &pitch, -90.0f, 90.0f, "Tilt %.0f°")) {
        clip.edit.pitch = pitch;
        m_project.dirty = true;
    }
    ImGui::SetNextItemWidth(w);
    float fov = static_cast<float>(clip.edit.fov);
    if (ImGui::SliderFloat("##fov", &fov, 30.0f, 170.0f, "FOV %.0f°")) {
        clip.edit.fov = fov;
        m_project.dirty = true;
    }

    ImGui::Dummy(ImVec2(0.0f, px(10.0f)));
    smallText("More controls (colour, stabilisation, view mode) arrive with the next steps.", p.text3);
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

// ---------------------------------------------------------------------------
//  Lay the four regions out.
// ---------------------------------------------------------------------------
void App::drawEditor(float x, float y, float width, float height) {
    const float gap = px(10.0f);
    const float binW = std::clamp(width * 0.18f, px(180.0f), px(280.0f));
    const float inspW = std::clamp(width * 0.24f, px(260.0f), px(380.0f));
    const float timelineH = std::clamp(height * 0.22f, px(110.0f), px(200.0f));
    const float topH = std::max(px(160.0f), height - timelineH - gap);
    const float centreW = std::max(px(200.0f), width - binW - inspW - 2.0f * gap);

    ImGui::SetCursorPos(ImVec2(x, y));
    drawMediaBin(ImVec2(binW, topH));

    ImGui::SetCursorPos(ImVec2(x + binW + gap, y));
    drawPreview(ImVec2(centreW, topH));

    ImGui::SetCursorPos(ImVec2(x + binW + gap + centreW + gap, y));
    drawInspector(ImVec2(inspW, topH));

    ImGui::SetCursorPos(ImVec2(x, y + topH + gap));
    drawTimeline(ImVec2(width, timelineH));
}

}  // namespace osvgui
