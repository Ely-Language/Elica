// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <cctype>

namespace Elica::Features::Preprocess {

struct ProtectedRegion {
    enum Kind {
        STRING, CHAR,
        MULTILINE,
        F_STRING, F_MULTILINE,
        COMMENT_LINE, COMMENT_BLOCK,
        CPP_CODE, C_CODE
    };

    Kind   kind;
    size_t start;
    size_t end;
};

class ProtectedRegions {
public:
    // Returns sorted by start regions
    static std::vector<ProtectedRegion> scan(const std::string &source);

    // region -> \x01<N>\x01 (N — outOriginals idx)
    static std::string mask(const std::string &source,
                            const std::vector<ProtectedRegion> &regions,
                            std::vector<std::string> &outOriginals);

    // unmask region
    static std::string unmask(const std::string &masked,
                              const std::vector<std::string> &originals);

    // [start, end) can be at the region
    static bool overlaps(const std::vector<ProtectedRegion> &regions,
                         size_t start, size_t end);

private:
    // "    " <- literaly is ident!
    static inline bool isIdent(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }
};

}