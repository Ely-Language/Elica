// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "lexer/token.h"

namespace Elica::Features {

// Toolchain probe: finds the system include directories of the configured
// compiler (default "g++", which the user has in PATH), so that
// `#include <stdio.h>`-style imports can be RESOLVED to real files.
// This is resolution only — no compilation happens here.
//
// manager.json may configure the compiler:
//   "compiler": { "path": "g++", "type": "g++" }
class Toolchain {
public:
    Toolchain(std::string compilerPath, std::string compilerType);

    // System include directories for C++ / C (probed once, cached).
    const std::vector<std::filesystem::path> &includeDirs(
        ::Elica::Lexer::LexerLanguage lang);

    // Resolves "<name>" against the system include paths; empty on miss.
    std::filesystem::path resolveInclude(
        const std::string &name, ::Elica::Lexer::LexerLanguage lang);

    const std::string &path() const { return path_; }
    const std::string &type() const { return type_; }

private:
    std::string path_;
    std::string type_;
    bool scanned_ = false;
    std::vector<std::filesystem::path> dirsCpp_;
    std::vector<std::filesystem::path> dirsC_;

    void runProbe(::Elica::Lexer::LexerLanguage lang,
                  std::vector<std::filesystem::path> &out);
};

} // namespace Elica::Features