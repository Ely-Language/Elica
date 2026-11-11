#include "features/macro.h"
#include "features/protected_regions.h"

using namespace std;
namespace fs = filesystem;

namespace Elica::Features::Preprocess::Macro {

namespace {

int parseBoolOrInt(const std::string &s) {
    if (s == "true"  || s == "1") return 1;
    if (s == "false" || s == "0") return 0;
    try { return std::stoi(s); }
    catch (...) { return 0; }
}

int parseIntStrict(const std::string &s) {
    try { return std::stoi(s); }
    catch (...) { return 0; }
}

const std::unordered_map<std::string, int(*)(const std::string &)> &parserRegistry() {
    // Register special parsers by macro name.
    // Unknown names → parseIntStrict (default stoi-like behavior).
    static const std::unordered_map<std::string, int(*)(const std::string &)> reg = {
        {"GC",  parseBoolOrInt},
        {"OPT", parseBoolOrInt},
    };
    return reg;
}

int parseMacroValue(const std::string &name, const std::string &valStr) {
    const auto &reg = parserRegistry();
    auto it = reg.find(name);
    if (it != reg.end()) return it->second(valStr);
    return parseIntStrict(valStr);
}

// ===============================================================
// helpers
// ===============================================================
inline bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
inline bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string trimCopy(const std::string &s) {
    size_t start = 0;
    size_t end   = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

// B2: identifiers that CANNOT be a leading type-position in <type> <name>.
// These are Ely keywords that are not types.
const std::unordered_set<std::string> &nonTypeKeywords() {
    static const std::unordered_set<std::string> s = {
        "func", "class", "struct",
        "public", "private", "mine", "static",
        "copa", "const", "nocopa", "sealed", "abstract",
        "wait", "unwait", "stack", "heap",
        "if", "else", "while", "for", "return",
        "try", "raise", "except", "break", "continue",
        "using", "true", "false",
        "is", "in", "lambda", "not", "this",
        "cppCode", "ccode"
    };
    return s;
}

} // namespace

string MacroParser::applyHygiene(
    const string &body,
    const vector<string> &preserverNames
) {
    // protect cppCode/ccode/strings/comments inside the body.
    const auto regions = ::Elica::Features::Preprocess::ProtectedRegions::scan(body);

    auto isProtected = [&](size_t pos) {
        for (const auto &r : regions) {
            if (pos >= r.start && pos < r.end) return true;
            if (r.start > pos) break;
        }
        return false;
    };

    unordered_set<string> macroParams(preserverNames.begin(), preserverNames.end());
    unordered_set<string> localNames;

    auto record = [&](const string &name) {
        if (name.empty()) return;
        if (name == "main") return;          // never rename main
        if (macroParams.count(name)) return; // macro params are protected
        if (localNames.count(name)) return;
        localNames.insert(name);
    };

    const size_t n = body.size();
    size_t idx = 0;
    while (idx < n) {
        if (isProtected(idx)) { idx++; continue; }
        if (!isIdentStart(body[idx])) { idx++; continue; }

        size_t start = idx;
        while (idx < n && isIdentChar(body[idx])) idx++;
        const string word = body.substr(start, idx - start);

        // Skip whitespace.
        size_t after = idx;
        while (after < n && isspace(static_cast<unsigned char>(body[after]))) after++;

        auto readIdent = [&](size_t from) -> pair<string, size_t> {
            size_t s = from;
            size_t e = s;
            while (e < n && isIdentChar(body[e])) e++;
            return { body.substr(s, e - s), e };
        };

        // <type> func <name>
        if (word == "func") {
            if (after < n && isIdentStart(body[after]) && !isProtected(after)) {
                auto [name, nameEnd] = readIdent(after);
                record(name);
                idx = nameEnd;
                continue;
            }
        }

        // class|struct <name>
        if (word == "class" || word == "struct") {
            if (after < n && isIdentStart(body[after]) && !isProtected(after)) {
                auto [name, nameEnd] = readIdent(after);
                record(name);
                idx = nameEnd;
                continue;
            }
        }

        // <type> <name> ( = | ; | , | ) )
        // First identifier must not be a non-type keyword, otherwise this is
        // `return x;`, `if (x)`, etc.
        if (after < n && isIdentStart(body[after]) && !isProtected(after)
            && !macroParams.count(word)
            && !nonTypeKeywords().count(word)) {
            auto [name, nameEnd] = readIdent(after);
            if (!name.empty() && nameEnd > after && !isProtected(nameEnd)) {
                size_t tail = nameEnd;
                while (tail < n && isspace(static_cast<unsigned char>(body[tail]))) tail++;
                if (tail < n && (body[tail] == '=' || body[tail] == ';'
                              || body[tail] == ',' || body[tail] == ')')) {
                    record(name);
                    idx = nameEnd;
                    continue;
                }
            }
        }
    }

    // Rename pass — skip protected regions; A6 guarantees cppCode/ccode
    // and string/comment interiors stay untouched.
    string result;
    result.reserve(body.size());
    size_t i = 0;
    while (i < n) {
        if (isProtected(i)) { result += body[i++]; continue; }
        if (isIdentStart(body[i])) {
            size_t start = i;
            while (i < n && !isProtected(i) && isIdentChar(body[i])) i++;
            const string ident = body.substr(start, i - start);
            if (localNames.count(ident)) result += "___" + ident;
            else                          result += ident;
        } else {
            result += body[i++];
        }
    }
    return result;
}

shared_ptr<CompilerMacro> MacroParser::parseCompilerMacro(
    const string &source,
    ::Elica::Core::Position pos
) {
    size_t idx = 0;
    while (idx < source.size() && isspace(static_cast<unsigned char>(source[idx]))) idx++;
    if (idx >= source.size() || source[idx] != '#') return nullptr;
    idx++;

    while (idx < source.size() && isspace(static_cast<unsigned char>(source[idx]))) idx++;
    size_t nameStart = idx;
    while (idx < source.size() && isIdentChar(source[idx])) idx++;
    if (nameStart == idx) return nullptr;
    string name = source.substr(nameStart, idx - nameStart);

    while (idx < source.size() && isspace(static_cast<unsigned char>(source[idx]))) idx++;
    string valStr = (idx < source.size()) ? source.substr(idx) : "0";

    // Trim trailing whitespace and a single optional `;`.
    while (!valStr.empty() && isspace(static_cast<unsigned char>(valStr.back()))) valStr.pop_back();
    if (!valStr.empty() && valStr.back() == ';') valStr.pop_back();
    while (!valStr.empty() && isspace(static_cast<unsigned char>(valStr.back()))) valStr.pop_back();

    int value = parseMacroValue(name, trimCopy(valStr));
    return make_shared<CompilerMacro>(move(name), value, pos.line, pos.column);
}

shared_ptr<TextMacro> MacroParser::parseTextMacro(
    const string &source, 
    ::Elica::Core::Position pos
) {
     size_t idx = 0;
    while (idx < source.size() && isspace(static_cast<unsigned char>(source[idx]))) idx++;
    if (idx >= source.size() || source[idx] != '@') return nullptr;
    idx++;

    while (idx < source.size() && isspace(static_cast<unsigned char>(source[idx]))) idx++;

    // B6: read a base identifier. If it's `operator` and immediately followed
    // by `<...>`, consume the full operator signature as the macro name.
    size_t nameStart = idx;
    while (idx < source.size() && isIdentChar(source[idx])) idx++;
    if (nameStart == idx) return nullptr;
    string baseName = source.substr(nameStart, idx - nameStart);

    string opString;
    bool isOperator = false;
    if (baseName == "operator" && idx < source.size() && source[idx] == '<') {
        size_t opStart = idx + 1;
        size_t opEnd = source.find('>', opStart);
        if (opEnd == string::npos) return nullptr;
        opString = source.substr(opStart, opEnd - opStart);
        if (opString.empty()) return nullptr;
        idx = opEnd + 1;
        isOperator = true;
    }

    const string name = isOperator ? ("operator<" + opString + ">") : baseName;

    vector<string> args;
    bool hasArgs = false;
    while (idx < source.size()  && isspace(source[idx])) idx++;

    if (idx < source.size()  && source[idx] == '(') {
        hasArgs = true;
        idx++;
        while (idx < source.size()  && source[idx] != ')') {
            while (idx < source.size()  && isspace(source[idx])) idx++;
            size_t argStart = idx;
            while (idx < source.size()  && (isalnum(source[idx]) || source[idx] == '_')) {
                idx++;
            }
            if (argStart < idx) {
                args.push_back(source.substr(argStart, idx - argStart));
            }
            while (idx < source.size()  && (isspace(source[idx]) || source[idx] == ',')) {
                idx++;
            }
        }
        if (idx < source.size()  && source[idx] == ')') idx++;
    }

    while (idx < source.size()  && isspace(source[idx])) idx++;
    string rawBody = (idx < source.size()) ? source.substr(idx) : "";

    while (!rawBody.empty()  && isspace(rawBody.front())) rawBody.erase(rawBody.begin());
    while (!rawBody.empty()  && isspace(rawBody.back())) rawBody.pop_back();

    if (rawBody.size() >= 2  && rawBody.front() == '{'  && rawBody.back() == '}') {
        rawBody = rawBody.substr(1, rawBody.size() - 2);
        while (!rawBody.empty()  && isspace(rawBody.front())) rawBody.erase(rawBody.begin());
        while (!rawBody.empty()  && isspace(rawBody.back())) rawBody.pop_back();
    }

    string flattenedBody = "";
    bool lastWasSpace = false;
    for (char c : rawBody) {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (!lastWasSpace) {
                flattenedBody += ' ';
                lastWasSpace = true;
            }
        } else {
            flattenedBody += c;
            lastWasSpace = false;
        }
    }
    while (!flattenedBody.empty()  && isspace(flattenedBody.front())) flattenedBody.erase(flattenedBody.begin());
    while (!flattenedBody.empty()  && isspace(flattenedBody.back())) flattenedBody.pop_back();

    rawBody = flattenedBody;

    string hygienicBody = hasArgs ? applyHygiene(rawBody, args) : rawBody;

    shared_ptr<TextMacro> result = hasArgs
        ? make_shared<TextMacro>(name, move(hygienicBody), move(args), pos.line, pos.column)
        : make_shared<TextMacro>(name, move(hygienicBody), pos.line, pos.column);

    if (isOperator) {
        result->setOperator(opString);
        // Operator macros are always function-like (2 params: lhs, rhs).
        // If the user wrote `@operator<^>(a,b)`, hasArgs is already true.
    }
    return result;
}

shared_ptr<StructMacro> MacroParser::parseStructMacro(
    const string &source, 
    ::Elica::Core::Position pos
) {
    size_t idx = 0;
    while (idx < source.size()  && isspace(source[idx])) idx++;
    if (idx >= source.size() || source[idx] != '&') return nullptr;
    idx++;

    while (idx < source.size()  && isspace(source[idx])) idx++;
    size_t nameStart = idx;
    while (idx < source.size()  && (isalnum(source[idx]) || source[idx] == '_')) {
        idx++;
    }
    if (nameStart == idx) return nullptr;
    string name = source.substr(nameStart, idx - nameStart);

    auto structMacro = make_shared<StructMacro>(name, pos);

    while (idx < source.size()  && isspace(source[idx])) idx++;
    if (idx < source.size()  && source[idx] == '(') {
        idx++;
        while (idx < source.size()  && source[idx] != ')') {
            while (idx < source.size()  && isspace(source[idx])) idx++;
            size_t pStart = idx;
            while (idx < source.size()  && source[idx] != ','  && source[idx] != ')'  && !isspace(source[idx])) {
                idx++;
            }
            if (pStart < idx) {
                structMacro->addPlaceholder(source.substr(pStart, idx - pStart));
            }
            while (idx < source.size()  && (isspace(source[idx]) || source[idx] == ',')) {
                idx++;
            }
        }
        if (idx < source.size()  && source[idx] == ')') idx++;
    }

    while (idx < source.size()  && isspace(source[idx])) idx++;
    if (idx < source.size()  && source[idx] == '=') {
        idx++;
        while (idx < source.size()  && isspace(source[idx])) idx++;
        
        size_t targetStart = idx;
        while (idx < source.size()  && source[idx] != '{'  && source[idx] != ';'  && !isspace(source[idx])) {
            idx++;
        }
        string targetType = source.substr(targetStart, idx - targetStart);
        
        structMacro->setTargetType(targetType);
    }

    while (idx < source.size()  && isspace(source[idx])) idx++;
    if (idx < source.size()  && source[idx] == '{') {
        idx++;
        while (idx < source.size()  && source[idx] != '}') {
            while (idx < source.size()  && isspace(source[idx])) idx++;
            
            size_t keyStart = idx;
            while (idx < source.size()  && (isalnum(source[idx]) || source[idx] == '_')) idx++;
            string key = source.substr(keyStart, idx - keyStart);

            while (idx < source.size()  && isspace(source[idx])) idx++;
            if (idx < source.size()  && source[idx] == '=') {
                idx++;
                while (idx < source.size()  && isspace(source[idx])) idx++;
                if (idx < source.size()  && source[idx] == '{') {
                    idx++;
                    size_t quoteStart = idx;
                    int depth = 1;
                    while (idx < source.size()  && depth > 0) {
                        if (source[idx] == '{') depth++;
                        else if (source[idx] == '}') depth--;
                        if (depth > 0) idx++;
                    }
                    string quoteBody = source.substr(quoteStart, idx - quoteStart);
                    if (idx < source.size()  && source[idx] == '}') idx++;

                    if (key == "preQuotes") structMacro->setPreQuotes(quoteBody);
                    else if (key == "beginQuotes") structMacro->setBeginQuotes(quoteBody);
                    else if (key == "endQuotes") structMacro->setEndQuotes(quoteBody);
                    else if (key == "postQuotes") structMacro->setPostQuotes(quoteBody);
                }
            }

            while (idx < source.size()  && (isspace(source[idx]) || source[idx] == ',' || source[idx] == ';')) {
                idx++;
            }
        }
    }

    return structMacro;
}

shared_ptr<Macro> MacroParser::parse(const string &source, ::Elica::Core::Position pos) {
    size_t idx = 0;
    while (idx < source.size()  && isspace(source[idx])) idx++;
    if (idx >= source.size()) return nullptr;

    char prefix = source[idx];
    switch (prefix) {
        case '#':
            return parseCompilerMacro(source, pos);
        case '@':
            return parseTextMacro(source, pos);
        case '&':
            return parseStructMacro(source, pos);
        default:
            return nullptr;
    }
}

}