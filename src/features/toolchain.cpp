// SPDX-License-Identifier: MIT
#include "features/toolchain.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace Elica::Features {

Toolchain::Toolchain(std::string compilerPath, std::string compilerType)
    : path_(std::move(compilerPath)), type_(std::move(compilerType)) {
    if (path_.empty()) path_ = "g++";
    if (type_.empty()) type_ = "g++";
}

void Toolchain::runProbe(::Elica::Lexer::LexerLanguage lang,
                         std::vector<fs::path> &out) {
    const char *xlang = (lang == ::Elica::Lexer::LexerLanguage::C) ? "c" : "c++";
    // `-` tells the compiler to read the program from stdin; without
    // redirecting stdin it would block forever waiting for EOF.
    // popen() goes through the platform shell, so use the OS null device.
#ifdef _WIN32
    const char *nullDev = "NUL";
#else
    const char *nullDev = "/dev/null";
#endif
    const std::string cmd =
        "\"" + path_ + "\" -E -x " + xlang + " -v - < " + nullDev + " 2>&1";

    std::string all;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;
    char buf[4096];
    // Safety net: never read forever — a hanging compiler must not hang the
    // whole pipeline. The probe output is small (<64 KiB), so cap the read.
    size_t total = 0;
    while (fgets(buf, sizeof(buf), pipe) && total < (1u << 20)) {
        all += buf;
        total += strlen(buf);
    }
    pclose(pipe);

    bool inSearch = false;
    std::istringstream ss(all);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("search starts here:") != std::string::npos) {
            inSearch = true;
            continue;
        }
        if (inSearch && line.find("End of search list.") != std::string::npos) break;
        if (!inSearch) continue;

        // trim
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t");
        if (e == std::string::npos) continue;
        std::string dir = line.substr(b, e - b + 1);

        if (dir.find("(framework directory)") != std::string::npos) continue;
        if (dir.find("ignoring") != std::string::npos) continue;

        fs::path p(dir);
        std::error_code ec;
        if (fs::is_directory(p, ec) && !ec) out.push_back(p);
    }
}

const std::vector<fs::path> &Toolchain::includeDirs(
    ::Elica::Lexer::LexerLanguage lang) {
    std::vector<fs::path> &target =
        (lang == ::Elica::Lexer::LexerLanguage::C) ? dirsC_ : dirsCpp_;
    if (target.empty()) runProbe(lang, target);
    return target;
}

fs::path Toolchain::resolveInclude(const std::string &name,
                                  ::Elica::Lexer::LexerLanguage lang) {
    for (const auto &dir : includeDirs(lang)) {
        const fs::path candidate = dir / name;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec) && !ec) return candidate;
    }
    return {};
}

} // namespace Elica::Features