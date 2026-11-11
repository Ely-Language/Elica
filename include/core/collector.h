// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>

#include "nlohmann/json.hpp"
#include "core/utils.h"

#include "features/macro.h"


namespace fs = std::filesystem;

namespace Elica::Features {
namespace Preprocess {
class Preprocessor;
namespace Macro {
class Macro;
class CompilerMacro;
class TextMacro;
class StructMacro;
}
}
}

namespace Elica::Core {

// Configuration
struct ProjectConfig {
    std::string name;
    std::string version;
    fs::path entry;
    std::unordered_map<std::string, int> settings;
};

struct ModuleInfo {
    std::string name;
    std::vector<fs::path> elycFiles; 
    std::vector<fs::path> headerFiles;
    std::vector<fs::path> libraryFiles;
};
    
enum FileType : uint8_t {
    ELY, ELYC, CPP, C, HPP, H, DLL, SO, dylib, LIB, A 
};

// Ely Project Representation
class Project {
private:
    fs::path rootDir;
    ProjectConfig config;

    std::vector<Error> errors;

    // ely, elyc
    std::vector<fs::path> elySourceFiles;
    // native sources (c, cpp)
    std::vector<fs::path> nativeSourceFiles;
    // native headers (h, hpp)
    std::vector<fs::path> nativeHeaderFiles;
    // precompiled libs (.dll, .lib, .a, .so etc)
    std::vector<fs::path> externalLibs;

    // macrosystem
    
    // #
    std::vector<::Elica::Features::Preprocess::Macro::CompilerMacro> compilerMacros;
    // @
    std::vector<::Elica::Features::Preprocess::Macro::TextMacro> textMacros;
    // &
    std::vector<::Elica::Features::Preprocess::Macro::StructMacro> structMacros;

    fs::path locateModule(const std::string &moduleName) const;

    friend class Elica::Features::Preprocess::Preprocessor;
public:
    explicit Project(fs::path rootDir);

    bool loadConfig();

    void addFile(fs::path path, FileType type);

    bool collectSources();

    bool addLib(const std::string &moduleName, ModuleInfo &outInfo);

    std::string getRawFile(fs::path filePath);

    const ProjectConfig &getConfig() const { return this->config; }
    const fs::path &getRootDir() const { return this->rootDir; }

    const std::vector<fs::path> &getElySourceFiles() const { return this->elySourceFiles; }
    const std::vector<fs::path> &getNativeSourceFiles() const { return this->nativeSourceFiles; }
    const std::vector<fs::path> &getNativeHeaderFiles() const { return this->nativeHeaderFiles; }
    const std::vector<fs::path> &getExternalLibraries() const { return this->externalLibs; }

    void pushError(Error e) { this->errors.push_back(e); }

    int traceErrors();
};

}