// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The OpenOSV / Avata 360 Editor Contributors
//
// Project.cpp - load, save and build the editor's document model (Project.h).

#include "Project.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace osvgui {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

/// A per-process counter so two saves never share a temporary file name.
std::atomic<unsigned> g_saveCounter{0};

/// Read a string field if present and of the right type; leave `out` otherwise.
void readString(const Json& obj, const char* key, std::string& out) {
    if (const auto it = obj.find(key); it != obj.end() && it->is_string()) {
        out = it->get<std::string>();
    }
}

/// Read an integer field if present; leave `out` otherwise.
void readInt64(const Json& obj, const char* key, std::int64_t& out) {
    if (const auto it = obj.find(key); it != obj.end() && it->is_number_integer()) {
        out = it->get<std::int64_t>();
    }
}

}  // namespace

Project projectFromClips(const std::vector<fs::path>& sources, const GuiSettings& defaults) noexcept {
    Project project;
    project.output = defaults;
    try {
        project.clips.reserve(sources.size());
        for (const fs::path& source : sources) {
            ProjectClip clip;
            clip.source = source;
            clip.name = pathToUtf8(source.filename());
            clip.edit = defaults;
            project.clips.push_back(std::move(clip));
        }
    } catch (...) {
        // Out of memory: return whatever was built so far rather than throw.
    }
    sanitiseProject(project);
    project.dirty = !project.clips.empty();
    return project;
}

std::string projectToJson(const Project& project) noexcept {
    try {
        Json root;
        root["schema"] = kProjectSchemaVersion;
        root["app"] = "Avata 360 Editor";

        // The project-wide output/encoding block is a whole GuiSettings, stored
        // as the nested JSON the settings serialiser already produces, so the
        // field mapping lives in exactly one place (Settings.cpp).
        root["output"] = Json::parse(settingsToJsonText(project.output), nullptr, false);
        if (root["output"].is_discarded()) {
            root["output"] = Json::object();
        }

        root["audioBed"] = pathToUtf8(project.audioBed);

        Json& clips = root["clips"];
        clips = Json::array();
        for (const ProjectClip& clip : project.clips) {
            Json c;
            c["source"] = pathToUtf8(clip.source);
            c["name"] = clip.name;
            Json edit = Json::parse(settingsToJsonText(clip.edit), nullptr, false);
            c["edit"] = edit.is_discarded() ? Json::object() : edit;
            c["inFrame"] = clip.inFrame;
            c["outFrame"] = clip.outFrame;
            if (!clip.keyframesJson.empty()) {
                Json kf = Json::parse(clip.keyframesJson, nullptr, false);
                if (!kf.is_discarded()) {
                    c["keyframes"] = kf;
                }
            }
            clips.push_back(std::move(c));
        }

        return root.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
    } catch (...) {
        return {};
    }
}

std::optional<Project> projectFromJson(const std::string& text, std::string* error) noexcept {
    try {
        const Json root = Json::parse(text.begin(), text.end(), nullptr, false, true);
        if (root.is_discarded() || !root.is_object()) {
            if (error) {
                *error = "not a JSON object";
            }
            return std::nullopt;
        }

        // Schema gate: read what this build understands, refuse a newer major.
        int schema = kProjectSchemaVersion;
        if (const auto it = root.find("schema"); it != root.end() && it->is_number_integer()) {
            schema = it->get<int>();
        }
        if (schema > kProjectSchemaVersion) {
            if (error) {
                *error = "this project was made by a newer version of Avata 360 Editor (schema " +
                         std::to_string(schema) + "); please update";
            }
            return std::nullopt;
        }

        Project project;

        if (const auto it = root.find("output"); it != root.end() && it->is_object()) {
            settingsFromJsonText(it->dump(), project.output, nullptr);
        }

        std::string audio;
        readString(root, "audioBed", audio);
        project.audioBed = pathFromUtf8(audio);

        if (const auto clipsIt = root.find("clips"); clipsIt != root.end() && clipsIt->is_array()) {
            project.clips.reserve(clipsIt->size());
            for (const Json& c : *clipsIt) {
                if (!c.is_object()) {
                    continue;
                }
                ProjectClip clip;
                std::string source;
                readString(c, "source", source);
                clip.source = pathFromUtf8(source);
                readString(c, "name", clip.name);
                if (const auto e = c.find("edit"); e != c.end() && e->is_object()) {
                    settingsFromJsonText(e->dump(), clip.edit, nullptr);
                } else {
                    clip.edit = project.output;
                }
                readInt64(c, "inFrame", clip.inFrame);
                readInt64(c, "outFrame", clip.outFrame);
                if (const auto k = c.find("keyframes"); k != c.end() && !k->is_null()) {
                    clip.keyframesJson = k->dump();
                }
                project.clips.push_back(std::move(clip));
            }
        }

        sanitiseProject(project);
        return project;
    } catch (...) {
        if (error) {
            *error = "could not parse the project file";
        }
        return std::nullopt;
    }
}

std::optional<Project> loadProject(const fs::path& file, std::string* error) noexcept {
    try {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            if (error) {
                *error = "cannot open " + pathToUtf8(file);
            }
            return std::nullopt;
        }
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::optional<Project> project = projectFromJson(text, error);
        if (project) {
            project->path = file;
            project->dirty = false;
        }
        return project;
    } catch (...) {
        if (error) {
            *error = "could not read " + pathToUtf8(file);
        }
        return std::nullopt;
    }
}

bool saveProject(Project& project, const fs::path& file, std::string* error) noexcept {
    try {
        const std::string text = projectToJson(project);
        if (text.empty()) {
            if (error) {
                *error = "could not serialise the project";
            }
            return false;
        }

        std::error_code ec;
        if (file.has_parent_path()) {
            fs::create_directories(file.parent_path(), ec);
        }

#if defined(_WIN32)
        const unsigned long pid = static_cast<unsigned long>(::GetCurrentProcessId());
#else
        const unsigned long pid = static_cast<unsigned long>(::getpid());
#endif
        fs::path temp = file;
        temp += ".tmp-" + std::to_string(pid) + "-" + std::to_string(g_saveCounter.fetch_add(1));
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) {
                if (error) {
                    *error = "cannot write " + pathToUtf8(temp);
                }
                return false;
            }
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            out.flush();
            if (!out) {
                out.close();
                fs::remove(temp, ec);
                if (error) {
                    *error = "cannot write " + pathToUtf8(temp);
                }
                return false;
            }
        }
        // Retry the rename briefly: a scanner or editor holding the old file
        // can make it fail for a moment.
        for (int attempt = 0; attempt < 10; ++attempt) {
            fs::rename(temp, file, ec);
            if (!ec) {
                project.path = file;
                project.dirty = false;
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        fs::remove(temp, ec);
        if (error) {
            *error = "cannot replace " + pathToUtf8(file);
        }
        return false;
    } catch (...) {
        if (error) {
            *error = "could not save the project";
        }
        return false;
    }
}

void sanitiseProject(Project& project) noexcept {
    sanitise(project.output);
    for (ProjectClip& clip : project.clips) {
        sanitise(clip.edit);
        if (clip.name.empty()) {
            clip.name = pathToUtf8(clip.source.filename());
            if (clip.name.empty()) {
                clip.name = "clip";
            }
        }
        if (clip.inFrame < -1) {
            clip.inFrame = -1;
        }
        if (clip.outFrame < -1) {
            clip.outFrame = -1;
        }
    }
}

}  // namespace osvgui
