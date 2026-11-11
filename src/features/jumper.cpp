// SPDX-License-Identifier: MIT
#include "features/jumper.h"
#include "features/protected_regions.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include "core/collector.h"
#include "nlohmann/json.hpp"

using namespace std;
namespace fs = filesystem;

namespace Elica::Core {
    class Project;
}

namespace Elica::Features::Preprocess::Jumping {

namespace {

bool isIdentChar(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

string trimCopy(const string &s) {
    size_t start = 0;
    size_t end = s.size();
    while (start < end && isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

vector<string> splitDots(const string &s) {
    vector<string> parts;
    string cur;
    for (char c : s) {
        if (c == '.') {
            if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

bool isElyFile(const fs::path &p) {
    const auto ext = p.extension().string();
    return ext == ".ely" || ext == ".elyc";
}

bool isCppSource(const fs::path &p) {
    const auto ext = p.extension().string();
    return ext == ".cpp" || ext == ".cc" || ext == ".cxx";
}

bool isCSource(const fs::path &p) {
    return p.extension() == ".c";
}

bool isHeader(const fs::path &p) {
    const auto ext = p.extension().string();
    return ext == ".h" || ext == ".hpp";
}

} // namespace

Jumper::Jumper(::Elica::Core::Project &proj) : project(proj) {}

// ---------------------------------------------------------------
// File registration & recursion
// ---------------------------------------------------------------

bool Jumper::addFileIfNew(const fs::path &file) {
    std::error_code ec;
    const fs::path normalized = fs::weakly_canonical(file, ec);
    if (ec) return false;
    if (visited.count(normalized)) return false;
    visited.insert(normalized);

    const auto ext = file.extension().string();
    if (ext == ".ely") {
        project.addFile(file, ::Elica::Core::FileType::ELY);
    } else if (ext == ".elyc") {
        project.addFile(file, ::Elica::Core::FileType::ELYC);
    } else if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") {
        project.addFile(file, ::Elica::Core::FileType::CPP);
    } else if (ext == ".c") {
        project.addFile(file, ::Elica::Core::FileType::C);
    } else if (ext == ".h") {
        project.addFile(file, ::Elica::Core::FileType::H);
    } else if (ext == ".hpp") {
        project.addFile(file, ::Elica::Core::FileType::HPP);
    } else {
        return false;
    }

    return visitFile(file);
}

bool Jumper::visitFile(const fs::path &file) {
    const string source = project.getRawFile(file);
    if (source.empty()) return false;
    scanImports(file, source);
    return true;
}

// ---------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------

void Jumper::scanImports(const fs::path &file, const string &source) {
    const auto regions = ProtectedRegions::scan(source);
    vector<string> originals;
    const string masked = ProtectedRegions::mask(source, regions, originals);

    if (isCppSource(file) || isCSource(file) || isHeader(file)) {
        // C/C++: only #include (no cppCode/cCode inside these files).
        scanIncludes(masked, file);
        return;
    }

    if (isElyFile(file)) {
        // Ely: `using` in the masked body.
        scanUsings(masked, file);

        // #include only lives inside cppCode/cCode blocks.
        for (size_t idx = 0; idx < regions.size(); ++idx) {
            const auto &r = regions[idx];
            if (r.kind != ProtectedRegion::CPP_CODE &&
                r.kind != ProtectedRegion::C_CODE) {
                continue;
            }
            // Re-mask the block to strip embedded strings/comments.
            const auto subRegions = ProtectedRegions::scan(originals[idx]);
            vector<string> subOriginals;
            const string subMasked =
                ProtectedRegions::mask(originals[idx], subRegions, subOriginals);
            scanIncludes(subMasked, file);
        }
    }
}

void Jumper::scanIncludes(const string &masked, const fs::path &fromFile) {
    size_t pos = 0;
    while ((pos = masked.find("#include", pos)) != string::npos) {
        size_t p = pos + 8;
        while (p < masked.size() && isspace(static_cast<unsigned char>(masked[p]))) p++;
        if (p >= masked.size()) break;

        const char opener = masked[p];

        // #include "local.h"
        if (opener == '"') {
            const size_t end = masked.find('"', p + 1);
            if (end != string::npos) {
                handleInclude(masked.substr(p + 1, end - p - 1), fromFile);
                pos = end + 1;
                continue;
            }
        }

        // #include <system.h> — try to resolve locally, else silently skip.
        if (opener == '<') {
            const size_t end = masked.find('>', p + 1);
            if (end != string::npos) {
                const string target = masked.substr(p + 1, end - p - 1);
                fs::path candidate = fromFile.parent_path() / target;
                if (!fs::exists(candidate)) {
                    candidate = project.getRootDir() / target;
                }
                if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
                    addFileIfNew(candidate);
                }
                pos = end + 1;
                continue;
            }
        }

        pos += 8;
    }
}

void Jumper::scanUsings(const string &masked, const fs::path &fromFile) {
    size_t pos = 0;
    while ((pos = masked.find("using", pos)) != string::npos) {
        const bool leftOk  = (pos == 0) || !isIdentChar(masked[pos - 1]);
        const size_t after = pos + 5;
        const bool rightOk = (after >= masked.size()) || !isIdentChar(masked[after]);
        if (!leftOk || !rightOk) { pos += 5; continue; }

        const size_t semi = masked.find(';', after);
        if (semi == string::npos) { pos += 5; continue; }

        const string target = trimCopy(masked.substr(after, semi - after));
        if (!target.empty()) handleUsing(target, fromFile);
        pos = semi + 1;
    }
}

// ---------------------------------------------------------------
// #include resolution
// ---------------------------------------------------------------

bool Jumper::handleInclude(const string &target, const fs::path &fromFile) {
    if (target.empty()) return false;

    // 1. relative to current file
    fs::path candidate = fromFile.parent_path() / target;
    
    // 2. relative to project root
    if (!fs::exists(candidate)) {
        candidate = project.getRootDir() / target;
        if (!fs::exists(candidate)) {
            project.pushError(::Elica::Core::Error(0, 0, fromFile,
                "Cannot resolve #include: \"" + target + "\""));
            return false;
        }
    }

    if (!fs::is_regular_file(candidate)) return false;
    return addFileIfNew(candidate);
}

// ---------------------------------------------------------------
// using resolution
// ---------------------------------------------------------------

bool Jumper::handleUsing(const string &target, const fs::path &fromFile) {
    if (target.empty()) return false;

    // using "folder/file";  |  using 'folder/file';
    const char first = target.front();
    if (first == '"' || first == '\'') {
        const char quote = first;
        const size_t end = target.find(quote, 1);
        if (end == string::npos) {
            project.pushError(::Elica::Core::Error(0, 0, fromFile,
                "Malformed using path: " + target));
            return false;
        }
        const string path = target.substr(1, end - 1);

        // 1. relative to current file
        fs::path candidate = fromFile.parent_path() / path;
        if (!fs::exists(candidate)) {
            // 2. relative to project root
            candidate = project.getRootDir() / path;
            if (!fs::exists(candidate)) {
                project.pushError(::Elica::Core::Error(0, 0, fromFile,
                    "Cannot resolve using path: \"" + path + "\""));
                return false;
            }
        }

        if (fs::is_regular_file(candidate)) {
            return addFileIfNew(candidate);
        }
        if (fs::is_directory(candidate)) {
            return loadModuleFiles(candidate, "", fromFile);
        }
        return false;
    }

    // using module;  |  using module.submodule;
    const auto parts = splitDots(target);
    if (parts.empty()) return false;

    const string &moduleName = parts[0];
    const string subKey = (parts.size() > 1) ? parts[1] : "";

    const fs::path moduleDir = resolveModuleDir(moduleName, fromFile);
    if (!moduleDir.empty()) {
        return loadModuleFiles(moduleDir, subKey, fromFile);
    }

    // Fallback for `using io;` inside console.elyc (F5 case):
    // a sibling file in the same directory.
    const fs::path base = fromFile.parent_path();
    const fs::path fallbackCandidates[] = {
        base / (moduleName + ".ely"),
        base / (moduleName + ".elyc"),
        base / moduleName,
    };
    for (const auto &cand : fallbackCandidates) {
        if (fs::exists(cand) && fs::is_regular_file(cand)) {
            return addFileIfNew(cand);
        }
    }

    project.pushError(::Elica::Core::Error(0, 0, fromFile,
        "Module not found: " + moduleName));
    return false;
}

fs::path Jumper::resolveModuleDir(const string &moduleName,
                                    const fs::path &fromFile) {
    // 1. modules/ at project root
    const fs::path local = project.getRootDir() / "modules" / moduleName;
    if (fs::exists(local) && fs::is_directory(local)) return local;

    // 2. walk-up from the current file (any ancestor's modules/<name>)
    fs::path walker = fromFile.parent_path();
    while (!walker.empty()) {
        const fs::path candidate = walker / "modules" / moduleName;
        if (fs::exists(candidate) && fs::is_directory(candidate)) return candidate;
        if (!walker.has_parent_path() || walker.parent_path() == walker) break;
        walker = walker.parent_path();
    }

    return {};
}

bool Jumper::loadModuleFiles(const fs::path &moduleDir,
                               const string &subKey,
                               const fs::path &fromFile) {
    const fs::path manifest = moduleDir / "module.json";
    if (!fs::exists(manifest)) {
        project.pushError(::Elica::Core::Error(0, 0, fromFile,
            "module.json missing in " + moduleDir.string()));
        return false;
    }

    try {
        ifstream in(manifest);
        nlohmann::json j;
        in >> j;

        vector<string> filesToAdd;

        // Legacy shape: { "sources": [...], ... }
        if (j.contains("sources") && j["sources"].is_array()) {
            for (const auto &s : j["sources"]) {
                filesToAdd.push_back(s.get<string>());
            }
        }

        // desdoc shape: named keys + "*"
        if (!subKey.empty()) {
            if (j.contains(subKey)) {
                const auto &v = j[subKey];
                if (v.is_string()) filesToAdd.push_back(v.get<string>());
                else if (v.is_array()) {
                    for (const auto &s : v) filesToAdd.push_back(s.get<string>());
                }
            }
        } else {
            if (j.contains("*") && j["*"].is_array()) {
                for (const auto &s : j["*"]) filesToAdd.push_back(s.get<string>());
            } else if (filesToAdd.empty()) {
                // Fallback: every .elyc / .ely in the module directory.
                for (const auto &entry : fs::directory_iterator(moduleDir)) {
                    const auto ext = entry.path().extension().string();
                    if (ext == ".elyc" || ext == ".ely") {
                        filesToAdd.push_back(entry.path().filename().string());
                    }
                }
            }
        }

        bool ok = false;
        for (const auto &fname : filesToAdd) {
            const fs::path full = moduleDir / fname;
            if (fs::exists(full)) {
                addFileIfNew(full);
                ok = true;
            } else {
                project.pushError(::Elica::Core::Error(0, 0, manifest,
                    "Module file not found: " + fname));
            }
        }
        return ok;
    } catch (const std::exception &e) {
        project.pushError(::Elica::Core::Error(0, 0, manifest,
            string("Failed to parse module.json: ") + e.what()));
        return false;
    }
}

// ---------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------

bool Jumper::jumpFrom(const fs::path &entry) {
    if (!fs::exists(entry)) {
        project.pushError(::Elica::Core::Error(0, 0, entry,
            "Entry file not found"));
        return false;
    }
    std::error_code ec;
    const fs::path normalized = fs::weakly_canonical(entry, ec);
    if (!ec && visited.count(normalized)) return true;
    addFileIfNew(entry);
    return true;
}

bool Jumper::jumpAll() {
    const auto &entry = project.getConfig().entry;
    if (entry.empty()) {
        project.pushError(::Elica::Core::Error(0, 0, project.getRootDir(),
            "manager.json has no \"enter\" field"));
        return false;
    }
    return jumpFrom(entry);
}

}