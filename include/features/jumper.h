// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "core/utils.h"
#include "features/protected_regions.h"

namespace Elica::Core { class Project; }

namespace Elica::Features::Preprocess::Jumping {

namespace fs = std::filesystem;

// Graph-based resolver for `using module;`, `using "path";` and `#include "..."`.
// Walks from entry point, adds reachable .ely/.elyc/.c/.cpp/.h/.hpp files
// into the Project, recursing through them.
class Jumper {
private:
    Elica::Core::Project &project;
    std::set<fs::path> visited;

    bool addFileIfNew(const fs::path &file);
    bool visitFile(const fs::path &file);

    void scanImports(const fs::path &file, const std::string &source);
    void scanIncludes(const std::string &masked, const fs::path &fromFile);
    void scanUsings(const std::string &masked, const fs::path &fromFile);

    bool handleInclude(const std::string &target, const fs::path &fromFile);
    bool handleUsing(const std::string &target, const fs::path &fromFile);

    fs::path resolveModuleDir(const std::string &moduleName,
                              const fs::path &fromFile);
    bool loadModuleFiles(const fs::path &moduleDir,
                         const std::string &subKey,
                         const fs::path &fromFile);

public:
    explicit Jumper(Elica::Core::Project &proj);
    ~Jumper() = default;

    // Walks from project.getConfig().entry.
    bool jumpAll();

    // Walks from an arbitrary entry file.
    bool jumpFrom(const fs::path &entry);
};

}