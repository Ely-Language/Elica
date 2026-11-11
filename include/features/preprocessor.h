// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

#include "core/utils.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "features/macro.h"


namespace fs = std::filesystem;

namespace Elica::Core { class Project; }

namespace Elica::Features::Preprocess {

class Preprocessor {
private:
    Elica::Core::Project* project;
    Elica::Lexer::Lexer   lexer;

    bool collected = false;

    // Find them
    void collectMacros(const std::string &source, const fs::path &filePath);

    // Vanish them
    void stripMacroDefinitions(std::string &source);

    // Paste them
    std::string expandTextMacros(const std::string &source, const fs::path &file);

    void replaceWithWhitespace(std::string &code, size_t start, size_t length);

public:
    explicit Preprocessor(Elica::Core::Project* proj);
    ~Preprocessor() = default;

    // Collect macros from all the sources
    void collectAll(const std::vector<fs::path> &files);

    // Onefile
    void collectFile(const fs::path &file, Elica::Lexer::LexerLanguage lang);

    // Preprocess
    std::vector<Elica::Lexer::Token> preprocess(const fs::path &file,
                                           Elica::Lexer::LexerLanguage lang);

    // Backward-compat: collectAll({file}) + preprocess(file).
    std::vector<Elica::Lexer::Token> process(const fs::path &file,
                                        Elica::Lexer::LexerLanguage lang);
};

}