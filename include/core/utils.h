// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

using namespace std;

namespace fs = filesystem;

namespace Elica::Core {

struct Position {
    size_t line;
    size_t column;

    Position(size_t l=0, size_t c=0) : line(l), column(c) {}
};

// Ely error representation
struct Error {
    Position position;
    fs::path path;
    string message;

    Error(size_t line, size_t column, const fs::path path, string message) {
        this->position = Position(line, column);
        this->path = path;
        this->message = message;
    }
};

// creates empty project
bool createProject(string rootPath, const string &projectName, const string &version = "0.0.1");

// checks file for IDEs
vector<Error> checkFileForIDE(const fs::path &root);

}