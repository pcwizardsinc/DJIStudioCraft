// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The OpenOSV / Avata 360 Editor Contributors
//
// Project.h - the document model for Avata 360 Editor.
//
// Phase 2 turns OpenOSV Studio (a batch queue) into an editor.  A Project is
// what the editor opens, edits and saves: an ordered list of clips, each with
// its own edit settings (framing, colour, stabilisation) and, from Phase 4, a
// keyframe track.  It serialises to a single UTF-8 JSON file with the
// extension `.avp` (Avata 360 Project), written atomically the same way the
// app's settings file is (Settings.cpp).
//
// The model deliberately reuses GuiSettings' per-render fields (mode, yaw,
// pitch, roll, fov, colour, stab, ...) as a clip's edit state, so the existing
// CommandBuilder can render a clip from its ClipEdit without a second mapping.
// Every entry point is noexcept: a malformed project on disk yields an error
// string and an empty project, never a throw into the UI loop.

#ifndef OSVGUI_PROJECT_H
#define OSVGUI_PROJECT_H

#include "Queue.h"     // ClipItem, path <-> UTF-8 helpers
#include "Settings.h"  // GuiSettings (the per-clip edit fields)

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace osvgui {

/// The file extension (with dot) of an Avata 360 Editor project.
inline constexpr const char* kProjectExtension = ".avp";

/// The on-disk schema version.  Bumped when a change can't be read by an older
/// build; the loader refuses a newer major and migrates an older one.
inline constexpr int kProjectSchemaVersion = 1;

/// One source clip in a project, plus every edit the user has made to it.
///
/// `edit` holds the clip's own copy of the render-relevant GuiSettings fields,
/// so two clips in one project can be framed and graded differently.  The
/// non-render GuiSettings fields (window, theme, tool paths) are ignored here;
/// they belong to the app, not the document.
struct ProjectClip {
    std::filesystem::path source;  ///< The .OSV (or Single Lens .MP4) itself.
    std::string name;              ///< Display name, UTF-8 (defaults to the file name).

    /// Per-clip edit state.  Reuses GuiSettings so CommandBuilder can consume
    /// it directly; only the render-relevant fields are read or written.
    GuiSettings edit;

    /// In/out points on the clip's own timeline, in frames; -1 means "the clip
    /// edge".  Phase 7 (trim) and Phase 8 (export ranges) use these; stored now
    /// so older projects round-trip once those phases land.
    std::int64_t inFrame = -1;
    std::int64_t outFrame = -1;

    /// Reserved for Phase 4: the clip's keyframe track, serialised verbatim so
    /// a project written by a keyframe-aware build still loads (and re-saves)
    /// losslessly in an earlier one.  Empty until Phase 4 defines its shape.
    std::string keyframesJson;
};

/// A whole project: an ordered list of clips and the project-wide settings
/// that aren't per-clip (output destination, encoder).  The ordering is the
/// timeline order the editor shows.
struct Project {
    /// Clips in timeline order.
    std::vector<ProjectClip> clips;

    /// Project-wide output and encoding choices (destination folder, codec,
    /// crf, audio, spherical metadata).  Per-clip framing/colour lives on each
    /// ProjectClip instead.
    GuiSettings output;

    /// An optional music/audio bed under the whole project (Phase 7).  Stored
    /// now so projects round-trip; unused until then.
    std::filesystem::path audioBed;

    /// True once a change has been made since the last save, so the UI can show
    /// a modified marker and prompt before closing.  Not serialised.
    bool dirty = false;

    /// Where this project was last saved or loaded, empty for a new unsaved
    /// project.  Not serialised (it IS the file's own location).
    std::filesystem::path path;

    [[nodiscard]] bool empty() const noexcept { return clips.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return clips.size(); }
};

/// Build a project from a set of source clips (e.g. the current queue), giving
/// each the supplied project-wide settings as its starting edit state.  Each
/// clip's `edit` is seeded from `defaults` so a brand-new project matches what
/// the batch queue would have rendered.
[[nodiscard]] Project projectFromClips(const std::vector<std::filesystem::path>& sources,
                                       const GuiSettings& defaults) noexcept;

/// Serialise a project to pretty-printed UTF-8 JSON (no trailing newline is
/// added beyond one).  Pure: it reads `project` and writes nothing to disk.
[[nodiscard]] std::string projectToJson(const Project& project) noexcept;

/// Parse a project from JSON text.  On success returns the project (with
/// `path` left empty - the caller sets it) and leaves `error` untouched; on
/// failure returns nullopt and, if `error` is non-null, a human-readable
/// reason.  A project whose schema major is newer than this build is refused.
[[nodiscard]] std::optional<Project> projectFromJson(const std::string& text, std::string* error) noexcept;

/// Load a project file, atomically-written sibling tolerated.  Sets the
/// returned project's `path` to `file`.  nullopt + `error` on any failure.
[[nodiscard]] std::optional<Project> loadProject(const std::filesystem::path& file, std::string* error) noexcept;

/// Write a project to `file` atomically (temp sibling + rename), the same way
/// saveSettings does, so a crash mid-save never truncates the project.  On
/// success clears `project.dirty` and sets `project.path` to `file`.  Returns
/// false + `error` on failure.
bool saveProject(Project& project, const std::filesystem::path& file, std::string* error) noexcept;

/// Bring a loaded project into range: every clip's edit settings are passed
/// through sanitise(), missing names are filled from the file name, and an
/// absent output block gets defaults.  Idempotent.
void sanitiseProject(Project& project) noexcept;

}  // namespace osvgui

#endif  // OSVGUI_PROJECT_H
