// SPDX-License-Identifier: MIT
#include "features/preprocessor.h"
#include "features/protected_regions.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

using namespace std;
namespace fs = filesystem;

namespace Elica::Features::Preprocess {

namespace {

struct ScannedMacro {
    char        prefix;
    size_t      start;
    size_t      end;
    size_t      line;
    size_t      column;
    string text;
};

string trimCopy(const string &s) {
    size_t start = 0;
    size_t end   = s.size();
    while (start < end && isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

// Один проход: находит позиции макроопределений, пропускает защищённые
// регионы и сбалансированные строки/комментарии внутри тел макросов.
vector<ScannedMacro> scanMacros(const string &src) {
    vector<ScannedMacro> result;
    auto regions = ProtectedRegions::scan(src);

    size_t idx = 0, line = 1, col = 1;
    const size_t n = src.size();

    auto advanceChar = [&](char c) {
        if (c == '\n') { line++; col = 1; }
        else col++;
    };

    while (idx < n) {
        // skip whitespace
        while (idx < n && isspace(static_cast<unsigned char>(src[idx]))) {
            advanceChar(src[idx]);
            idx++;
        }
        if (idx >= n) break;

        // skip protected region if we landed on it
        bool inside = false;
        for (const auto &r : regions) {
            if (idx >= r.start  && idx < r.end) { inside = true; break; }
            if (r.start > idx) break;
        }
        if (inside) {
            for (const auto &r : regions) {
                if (idx >= r.start  && idx < r.end) {
                    for (size_t j = idx; j < r.end; ++j) advanceChar(src[j]);
                    idx = r.end;
                    break;
                }
            }
            continue;
        }

        char prefix = src[idx];
        if (prefix != '#'  && prefix != '@'  && prefix != '&') {
            // skip line
            while (idx < n  && src[idx] != '\n') { advanceChar(src[idx]); idx++; }
            if (idx < n) { advanceChar(src[idx]); idx++; }
            continue;
        }

        size_t macroStart = idx;
        size_t macroLine  = line;
        size_t macroCol   = col;

        int  braceDepth  = 0;
        bool inBlock     = false;
        bool inStr       = false;
        char strChar     = 0;
        bool inLineCmt   = false;
        bool inBlockCmt  = false;

        while (idx < n) {
            char c = src[idx];

            if (inLineCmt) {
                if (c == '\n') inLineCmt = false;
                advanceChar(c); idx++;
                continue;
            }
            if (inBlockCmt) {
                if (c == '*'  && idx + 1 < n  && src[idx + 1] == '/') {
                    advanceChar(c); advanceChar('/'); idx += 2;
                    inBlockCmt = false;
                    continue;
                }
                advanceChar(c); idx++;
                continue;
            }
            if (inStr) {
                if (c == '\\'  && idx + 1 < n) {
                    advanceChar(c); advanceChar(src[idx + 1]); idx += 2;
                    continue;
                }
                if (c == strChar) inStr = false;
                advanceChar(c); idx++;
                continue;
            }

            if (c == '/'  && idx + 1 < n  && src[idx + 1] == '/') {
                advanceChar(c); advanceChar('/'); idx += 2;
                inLineCmt = true; continue;
            }
            if (c == '/'  && idx + 1 < n  && src[idx + 1] == '*') {
                advanceChar(c); advanceChar('*'); idx += 2;
                inBlockCmt = true; continue;
            }
            if (c == '"' || c == '\'') {
                inStr = true; strChar = c;
                advanceChar(c); idx++;
                continue;
            }

            if (c == '{') { braceDepth++; inBlock = true; }
            else if (c == '}') braceDepth--;

            if (c == '\n') {
                advanceChar(c);
                size_t checkIdx = idx;
                if (checkIdx > 0  && src[checkIdx - 1] == '\r') checkIdx--;
                bool hasBackslash = (checkIdx > macroStart  && src[checkIdx - 1] == '\\');
                if ((!inBlock  && !hasBackslash) ||
                    (inBlock  && braceDepth == 0)) {
                    idx++; break;
                }
                idx++;
                continue;
            }
            advanceChar(c); idx++;
        }

        result.push_back({prefix, macroStart, idx, macroLine, macroCol,
                          src.substr(macroStart, idx - macroStart)});
    }

    return result;
}

inline bool isIdentChar(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

inline bool isSymbolChar(char c) {
    return c == '=' || c == '<' || c == '>' || c == '!' ||
           c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '&' || c == '|' || c == '^' || c == '~';
}

// Scans backward from `end`, skips trailing whitespace, finds the start of
// the left atom. Atom = identifier chain (a.b.c) | number (1.5, 0x1F)
// | parenthesised group (...). Returns false if no atom found.
bool scanAtomBackward(const string &s, size_t end, size_t &outStart, size_t &outEnd) {
    while (end > 0 && isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    if (end == 0) return false;
    outEnd = end;

    const char last = s[end - 1];

    if (last == ')') {
        int depth = 0;
        size_t i = end;
        while (i > 0) {
            i--;
            if (s[i] == ')') depth++;
            else if (s[i] == '(') {
                depth--;
                if (depth == 0) { outStart = i; return true; }
            }
        }
        return false;
    }

    if (isIdentChar(last)) {
        size_t i = end;
        while (i > 0) {
            const char c = s[i - 1];
            if (isIdentChar(c) || c == '.') i--;
            else break;
        }
        outStart = i;
        return true;
    }
    return false;
}

// Scans forward from `start`, skips leading whitespace, finds the end of
// the right atom.
bool scanAtomForward(const string &s, size_t start, size_t &outStart, size_t &outEnd) {
    while (start < s.size() && isspace(static_cast<unsigned char>(s[start]))) start++;
    if (start >= s.size()) return false;
    outStart = start;

    const char first = s[start];

    if (first == '(') {
        int depth = 0;
        size_t i = start;
        while (i < s.size()) {
            if (s[i] == '(') depth++;
            else if (s[i] == ')') {
                depth--;
                if (depth == 0) { outEnd = i + 1; return true; }
            }
            i++;
        }
        return false;
    }

    if (isIdentChar(first)) {
        size_t i = start;
        while (i < s.size()) {
            const char c = s[i];
            if (isIdentChar(c) || c == '.') i++;
            else break;
        }
        outEnd = i;
        return true;
    }
    return false;
}

bool expandInfixOperators(
    string &processed,
    const unordered_map<string, vector<const Macro::TextMacro *>> &byOp
) {
    bool changed = false;

    for (const auto &[op, macros] : byOp) {
        if (op.empty()) continue;

        size_t pos = 0;
        while ((pos = processed.find(op, pos)) != string::npos) {
            const size_t opEnd = pos + op.size();

            // FIX: left guard — only skip if previous char is a symbol
            // (part of a compound operator like ^=, <<=). Digit/identifier
            // chars are the atom's trailing chars, handled by scanAtomBackward.
            if (pos > 0 && isSymbolChar(processed[pos - 1])) { pos++; continue; }

            // Skip if right char would extend this into a longer operator.
            if (opEnd < processed.size()) {
                const char rc = processed[opEnd];
                if (rc == '=' || rc == '<' || rc == '>' || rc == '+' || rc == '-'
                    || rc == '*' || rc == '/' || rc == '%' || rc == '&'
                    || rc == '|' || rc == '^') {
                    pos++; continue;
                }
            }

            size_t leftStart, leftEnd, rightStart, rightEnd;
            if (!scanAtomBackward(processed, pos, leftStart, leftEnd)) { pos++; continue; }
            if (!scanAtomForward(processed, opEnd, rightStart, rightEnd)) { pos++; continue; }

            const ::Elica::Features::Preprocess::Macro::TextMacro* macro = nullptr;
            for (const auto* m : macros) {
                if (m->getArgs().size() >= 2) { macro = m; break; }
            }
            if (!macro) { pos++; continue; }

            const string leftAtom  = processed.substr(leftStart,  leftEnd  - leftStart);
            const string rightAtom = processed.substr(rightStart, rightEnd - rightStart);

            string newBody = macro->getBody();
            const auto &params = macro->getArgs();

            auto substitute = [](string &s, const string &pname, const string &val) {
                string res;
                res.reserve(s.size());
                size_t i = 0;
                while (i < s.size()) {
                    if (s.compare(i, pname.size(), pname) == 0) {
                        const bool l = (i == 0)
                            || (!isalnum(static_cast<unsigned char>(s[i - 1])) && s[i - 1] != '_');
                        const bool r = (i + pname.size() == s.size())
                            || (!isalnum(static_cast<unsigned char>(s[i + pname.size()]))
                                && s[i + pname.size()] != '_');
                        if (l && r) {
                            res += val;
                            i += pname.size();
                            continue;
                        }
                    }
                    res += s[i++];
                }
                s = std::move(res);
            };
            substitute(newBody, params[0], leftAtom);
            substitute(newBody, params[1], rightAtom);

            const size_t oldLen = rightEnd - leftStart;
            const string oldSubstr = processed.substr(leftStart, oldLen);
            if (newBody == oldSubstr) { pos++; continue; }

            processed.replace(leftStart, oldLen, newBody);
            pos = leftStart + newBody.size();
            changed = true;
        }
    }

    return changed;
}

string stripBackslashContinuations(const string &text) {
    string clean = text;
    size_t p;
    while ((p = clean.find("\\\r\n")) != string::npos) clean.erase(p, 3);
    while ((p = clean.find("\\\n"))   != string::npos) clean.erase(p, 2);
    while (!clean.empty()  && isspace(static_cast<unsigned char>(clean.back()))) {
        if (clean.back() == '\\') { clean.pop_back(); break; }
        clean.pop_back();
    }
    return clean;
}

} // namespace

// ---------------------------------------------------------------

Preprocessor::Preprocessor(Elica::Core::Project* proj)
    : project(proj), lexer("", Elica::Lexer::LexerLanguage::ELY) {}

void Preprocessor::replaceWithWhitespace(string &code, size_t start, size_t length) {
    const size_t end = min(start + length, code.size());
    for (size_t i = start; i < end; ++i) {
        if (code[i] != '\n') code[i] = ' ';
    }
}

// ---------------- Phase 1 ---------------------------------------

void Preprocessor::collectMacros(const string &source, const fs::path &/*filePath*/) {
    for (const auto &m : scanMacros(source)) {
        ::Elica::Core::Position pos(m.line, m.column);
        auto parsed = Macro::MacroParser::parse(stripBackslashContinuations(m.text), pos);
        if (!parsed) continue;

        switch (m.prefix) {
            case '#':
                if (auto c = dynamic_pointer_cast<Macro::CompilerMacro>(parsed))
                    project->compilerMacros.push_back(*c);
                break;
            case '@':
                if (auto c = dynamic_pointer_cast<Macro::TextMacro>(parsed))
                    project->textMacros.push_back(*c);
                break;
            case '&':
                if (auto c = dynamic_pointer_cast<Macro::StructMacro>(parsed))
                    project->structMacros.push_back(*c);
                break;
        }
    }
}

void Preprocessor::collectFile(const fs::path &file, Elica::Lexer::LexerLanguage /*lang*/) {
    string source = project->getRawFile(file);
    if (source.empty()) return;
    collectMacros(source, file);
}

void Preprocessor::collectAll(const vector<fs::path> &files) {
    for (const auto &f : files) collectFile(f, Elica::Lexer::LexerLanguage::ELY);
    collected = true;
}

// ---------------- Phase 2 ---------------------------------------

void Preprocessor::stripMacroDefinitions(string &source) {
    auto macros = scanMacros(source);
    for (const auto &m : macros) {
        replaceWithWhitespace(source, m.start, m.end - m.start);
    }
}

string Preprocessor::expandTextMacros(const string &source, const fs::path &file) {
    auto regions = ProtectedRegions::scan(source);
    vector<string> originals;
    string processed = ProtectedRegions::mask(source, regions, originals);

    unordered_map<char, vector<const Macro::TextMacro *>> byFirstChar;
    unordered_map<string, vector<const Macro::TextMacro *>> byOp;
    for (const auto &m : project->textMacros) {
        if (m.isOperatorMacro()) {
            byOp[m.getOpString()].push_back(&m);
        } else {
            const auto &n = m.getName();
            if (!n.empty()) byFirstChar[n[0]].push_back(&m);
        }
    }

    const int maxIterations = 100;
    int iterations = 0;
    bool lastRoundChanged = false;

    while (iterations < maxIterations) {
        iterations++;
        bool anyThisRound = false;

        // B7: infix operator pass BEFORE regular expansion.
        if (expandInfixOperators(processed, byOp)) {
            anyThisRound = true;
        }

        size_t pos = 0;
        while (pos < processed.size()) {
            const char c = processed[pos];
            auto it = byFirstChar.find(c);
            if (it == byFirstChar.end()) { pos++; continue; }

            bool matched = false;
            for (const auto *macroPtr : it->second) {
                const auto &macro = *macroPtr;
                const string &name = macro.getName();

                if (pos + name.size() > processed.size()) continue;
                if (processed.compare(pos, name.size(), name) != 0) continue;

                const bool leftOk = (pos == 0)
                    || (!isalnum(static_cast<unsigned char>(processed[pos - 1]))
                        && processed[pos - 1] != '_');
                const bool rightOk = (pos + name.size() == processed.size())
                    || (!isalnum(static_cast<unsigned char>(processed[pos + name.size()]))
                        && processed[pos + name.size()] != '_');
                if (!leftOk || !rightOk) continue;

                if (macro.isFunctionlike()) {
                    size_t bp = pos + name.size();
                    while (bp < processed.size()
                           && isspace(static_cast<unsigned char>(processed[bp]))) bp++;
                    if (bp >= processed.size() || processed[bp] != '(') continue;

                    size_t endBracket = bp;
                    int depth = 0;
                    while (endBracket < processed.size()) {
                        if (processed[endBracket] == '(') depth++;
                        else if (processed[endBracket] == ')') {
                            depth--;
                            if (depth == 0) break;
                        }
                        endBracket++;
                    }
                    if (depth != 0) continue;

                    const size_t totalLen = (endBracket + 1) - pos;
                    const string callArgsStr =
                        processed.substr(bp + 1, endBracket - (bp + 1));

                    vector<string> callArgs;
                    {
                        string cur;
                        int pD = 0, bD = 0, sD = 0;
                        bool inS = false; char q = 0; bool esc = false;
                        for (char ch : callArgsStr) {
                            if (inS) {
                                if (esc) esc = false;
                                else if (ch == '\\') esc = true;
                                else if (ch == q) inS = false;
                                cur += ch; continue;
                            }
                            if (ch == '"' || ch == '\'') { inS = true; q = ch; cur += ch; continue; }
                            if (ch == '(') pD++;
                            else if (ch == ')') pD--;
                            else if (ch == '{') bD++;
                            else if (ch == '}') bD--;
                            else if (ch == '[') sD++;
                            else if (ch == ']') sD--;
                            if (ch == ',' && pD == 0 && bD == 0 && sD == 0) {
                                callArgs.push_back(trimCopy(cur));
                                cur.clear();
                            } else cur += ch;
                        }
                        string last = trimCopy(cur);
                        if (!last.empty()) callArgs.push_back(std::move(last));
                    }

                    string body = macro.getBody();
                    const auto &params = macro.getArgs();
                    for (size_t i = 0; i < params.size() && i < callArgs.size(); ++i) {
                        const string &pn = params[i];
                        const string &av = callArgs[i];
                        size_t pp = 0;
                        while ((pp = body.find(pn, pp)) != string::npos) {
                            const bool l = (pp == 0)
                                || (!isalnum(static_cast<unsigned char>(body[pp - 1]))
                                    && body[pp - 1] != '_');
                            const bool r = (pp + pn.size() == body.size())
                                || (!isalnum(static_cast<unsigned char>(body[pp + pn.size()]))
                                    && body[pp + pn.size()] != '_');
                            if (l && r) { body.replace(pp, pn.size(), av); pp += av.size(); }
                            else { pp += pn.size(); }
                        }
                    }

                    processed.replace(pos, totalLen, body);
                    pos += body.size();
                    anyThisRound = true;
                    matched = true;
                    break;
                } else {
                    const string &body = macro.getBody();
                    processed.replace(pos, name.size(), body);
                    pos += body.size();
                    anyThisRound = true;
                    matched = true;
                    break;
                }
            }

            if (!matched) pos++;
        }

        lastRoundChanged = anyThisRound;
        if (!anyThisRound) break;
    }

    if (lastRoundChanged && iterations >= maxIterations) {
        if (project) {
            project->pushError(::Elica::Core::Error(0, 0, file,
                "Macro expansion exceeded " + std::to_string(maxIterations)
                + " iterations (possible recursive @-macro definition)"));
        }
    }

    return ProtectedRegions::unmask(processed, originals);
}

vector<Elica::Lexer::Token> Preprocessor::preprocess(
        const fs::path &filePath, Elica::Lexer::LexerLanguage lang) {
    string source = project->getRawFile(filePath);
    if (source.empty()) {
        if (project)
            project->pushError(::Elica::Core::Error(0, 0, filePath,
                                                    "Failed to read file for preprocessing."));
        return {};
    }

    stripMacroDefinitions(source);

    string expanded = expandTextMacros(source, filePath);

    lexer.setfull(expanded, lang);
    return lexer.tokenize();
}

vector<Elica::Lexer::Token> Preprocessor::process(
        const fs::path &filePath, Elica::Lexer::LexerLanguage lang) {
    if (!collected) collectAll({filePath});
    return preprocess(filePath, lang);
}

}