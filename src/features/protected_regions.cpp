// utf-8 with erombombom?
#include "features/protected_regions.h"

namespace Elica::Features::Preprocess {

using PR = ProtectedRegion;

std::vector<PR> ProtectedRegions::scan(const std::string &s) {
    std::vector<PR> out;
    const size_t n = s.size();
    size_t i = 0;

    while (i < n) {
        char c = s[i];

        // // line comment
        if (c == '/'  && i + 1 < n  && s[i + 1] == '/') {
            size_t st = i; i += 2;
            while (i < n  && s[i] != '\n') i++;
            out.push_back({PR::COMMENT_LINE, st, i});
            continue;
        }
        // /* block */
        if (c == '/'  && i + 1 < n  && s[i + 1] == '*') {
            size_t st = i; i += 2;
            while (i + 1 < n  && !(s[i] == '*'  && s[i + 1] == '/')) i++;
            if (i + 1 < n) i += 2; else i = n;
            out.push_back({PR::COMMENT_BLOCK, st, i});
            continue;
        }

        // f-string / f-multiline
        bool prevIsIdent = i > 0  && isIdent(s[i - 1]);
        if ((c == 'f' || c == 'F')  && !prevIsIdent  && i + 1 < n  && s[i + 1] == '"') {
            if (i + 3 < n  && s[i + 2] == '"'  && s[i + 3] == '"') {
                size_t st = i; i += 4;
                while (i + 2 < n  && !(s[i] == '"'  && s[i + 1] == '"'  && s[i + 2] == '"')) i++;
                if (i + 2 < n) i += 3; else i = n;
                out.push_back({PR::F_MULTILINE, st, i});
            } else {
                size_t st = i; i += 2;
                while (i < n  && s[i] != '"') { if (s[i] == '\\') i++; if (i < n) i++; }
                if (i < n) i++;
                out.push_back({PR::F_STRING, st, i});
            }
            continue;
        }

        // """multiline"""
        if (c == '"'  && i + 2 < n  && s[i + 1] == '"'  && s[i + 2] == '"') {
            size_t st = i; i += 3;
            while (i + 2 < n  && !(s[i] == '"'  && s[i + 1] == '"'  && s[i + 2] == '"')) i++;
            if (i + 2 < n) i += 3; else i = n;
            out.push_back({PR::MULTILINE, st, i});
            continue;
        }

        // "string"
        if (c == '"') {
            size_t st = i; i++;
            while (i < n  && s[i] != '"') { if (s[i] == '\\') i++; if (i < n) i++; }
            if (i < n) i++;
            out.push_back({PR::STRING, st, i});
            continue;
        }

        // 'char'
        if (c == '\'') {
            size_t st = i; i++;
            while (i < n  && s[i] != '\'') { if (s[i] == '\\') i++; if (i < n) i++; }
            if (i < n) i++;
            out.push_back({PR::CHAR, st, i});
            continue;
        }

        // cppCode/cCode { ... }
        if (c == 'c'  && !prevIsIdent) {
            bool isCpp = (i + 7 <= n)  && s.compare(i, 7, "cppCode") == 0
                          && (i + 7 == n || !isIdent(s[i + 7]));
            bool isC   = (i + 5 <= n)  && s.compare(i, 5, "cCode")   == 0
                          && (i + 5 == n || !isIdent(s[i + 5]));

            if (isCpp || isC) {
                size_t keywordLen = isCpp ? 7 : 5;
                size_t st = i;
                size_t j  = i + keywordLen;
                while (j < n  && std::isspace(static_cast<unsigned char>(s[j]))) j++;
                if (j < n  && s[j] == '{') {
                    j++;
                    int depth = 1;
                    while (j < n  && depth > 0) {
                        if (s[j] == '"' || s[j] == '\'') {
                            char q = s[j++];
                            while (j < n  && s[j] != q) { if (s[j] == '\\') j++; if (j < n) j++; }
                            if (j < n) j++;
                            continue;
                        }
                        if (s[j] == '/'  && j + 1 < n  && s[j + 1] == '/') {
                            while (j < n  && s[j] != '\n') j++; continue;
                        }
                        if (s[j] == '/'  && j + 1 < n  && s[j + 1] == '*') {
                            j += 2;
                            while (j + 1 < n  && !(s[j] == '*'  && s[j + 1] == '/')) j++;
                            if (j + 1 < n) j += 2;
                            continue;
                        }
                        if (s[j] == '{') depth++;
                        else if (s[j] == '}') depth--;
                        j++;
                    }
                    out.push_back({isCpp ? PR::CPP_CODE : PR::C_CODE, st, j});
                    i = j;
                    continue;
                }
            }
        }

        i++;
    }
    return out;
}

bool ProtectedRegions::overlaps(const std::vector<PR> &rs, size_t st, size_t en) {
    for (const auto &r : rs) {
        if (r.start >= en) return false;
        if (r.end > st  && r.start < en) return true;
    }
    return false;
}

std::string ProtectedRegions::mask(const std::string &src,
                                   const std::vector<PR> &rs,
                                   std::vector<std::string> &originals) {
    if (rs.empty()) return src;
    originals.clear();
    originals.reserve(rs.size());

    std::string res;
    res.reserve(src.size());

    size_t lastEnd = 0;
    for (size_t idx = 0; idx < rs.size(); ++idx) {
        const auto &r = rs[idx];
        res.append(src, lastEnd, r.start - lastEnd);
        originals.push_back(src.substr(r.start, r.end - r.start));
        res += '\x01';
        res += std::to_string(idx);
        res += '\x01';
        lastEnd = r.end;
    }
    res.append(src, lastEnd, src.size() - lastEnd);
    return res;
}

std::string ProtectedRegions::unmask(const std::string &masked,
                                     const std::vector<std::string> &originals) {
    std::string res;
    res.reserve(masked.size());

    size_t i = 0;
    while (i < masked.size()) {
        if (masked[i] == '\x01') {
            size_t j = i + 1;
            while (j < masked.size()  && masked[j] != '\x01') j++;
            if (j < masked.size()) {
                try {
                    size_t idx = std::stoul(masked.substr(i + 1, j - i - 1));
                    if (idx < originals.size()) {
                        res += originals[idx];
                        i = j + 1;
                        continue;
                    }
                } catch (...) {}
            }
        }
        res += masked[i++];
    }
    return res;
}

}