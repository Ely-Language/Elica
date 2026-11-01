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

using namespace std;
namespace fs = filesystem;

namespace Elica::Core {
    class Project;
}

namespace Elica::Features::Preprocess {

class Preprocessor {
private:
    Elica::Core::Project* project;
    
    Elica::Lexer::Lexer lexer;

    // founds macros from file
    void collectMacros(string& source, const fs::path& filePath);

    // recursive macro expansion
    string expandTextMacros(const string& source, const fs::path file);

    // keep code good
    void replaceWithWhitespace(string& code, size_t start, size_t length);

public:
    explicit Preprocessor(Elica::Core::Project* proj);

    ~Preprocessor() = default;

    // process the code
    vector<Elica::Lexer::Token> process(const filesystem::path& filePath, Elica::Lexer::LexerLanguage lang);
};

}