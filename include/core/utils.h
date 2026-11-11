// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

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
    std::string message;

    Error(size_t line, size_t column, const fs::path path, std::string message) {
        this->position = Position(line, column);
        this->path = path;
        this->message = message;
    }
};

// creates empty project
bool createProject(std::string rootPath, const std::string &projectName, const std::string &version = "0.0.1");

// checks file for IDEs
std::vector<Error> checkFileForIDE(const fs::path &root);


template <typename V>
class OrderedMap {
public:
    using Container    = std::vector<std::pair<std::string, V>>;
    using iterator     = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    V &operator[](const std::string &key) {
        for (auto &p : items_) if (p.first == key) return p.second;
        items_.emplace_back(key, V{});
        return items_.back().second;
    }

    iterator       begin()       { return items_.begin(); }
    iterator       end()         { return items_.end();   }
    const_iterator begin() const { return items_.begin(); }
    const_iterator end()   const { return items_.end();   }

    size_t          size()  const { return items_.size();  }
    bool            empty() const { return items_.empty(); }
    const Container &getItems() const { return items_; }

private:
    Container items_;
};

}