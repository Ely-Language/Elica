#include "features/macro.h"

namespace Elica::Features::Preprocess::Macro {

string MacroParser::applyHygiene(
    const string& body, 
    const vector<string>& preserverNames
) {
    unordered_set<string> macroParams(preserverNames.begin(), preserverNames.end());
    unordered_set<string> localVars;
    
    static const unordered_set<string> types = {
        "byte", "ubyte", "int", "uint", "short", "ushort", "more", "umore",
        "flt", "double", "noised", "str", "atom", "char", "bool", "void", "class", "struct"
    };

    size_t idx = 0;
    while (idx < body.size()) {
        char c = body[idx];
        if (isalpha(c) || c == '_') {
            size_t start = idx;
            while (idx < body.size() && (isalnum(body[idx]) || body[idx] == '_')) idx++;
            string word = body.substr(start, idx - start);

            if (types.find(word) != types.end()) {
                while (idx < body.size() && isspace(body[idx])) idx++;
                size_t nameStart = idx;
                while (idx < body.size() && (isalnum(body[idx]) || body[idx] == '_')) idx++;
                if (nameStart < idx) {
                    string varName = body.substr(nameStart, idx - nameStart);
                    if (types.find(varName) == types.end() && macroParams.find(varName) == macroParams.end()) {
                        localVars.insert(varName);
                    }
                }
            }
        } else {
            idx++;
        }
    }

    string result;
    result.reserve(body.size());
    
    size_t i = 0;
    while (i < body.size()) {
        char c = body[i];
        if (isalpha(c) || c == '_') {
            size_t start = i;
            while (i < body.size() && (isalnum(body[i]) || body[i] == '_')) i++;
            string identifier = body.substr(start, i - start);
            
            if (localVars.find(identifier) != localVars.end()) {
                result += "___" + identifier;
            } else {
                result += identifier;
            }
        } else {
            result += c;
            i++;
        }
    }
    return result;
}

shared_ptr<CompilerMacro> MacroParser::parseCompilerMacro(
    const string& source, 
    ::Elica::Core::Position pos
) {
    size_t idx = 0;
    while (idx < source.size() && isspace(source[idx])) idx++;
    if (idx >= source.size() || source[idx] != '#') return nullptr;
    idx++;

    while (idx < source.size() && isspace(source[idx])) idx++;
    size_t nameStart = idx;
    while (idx < source.size() && (isalnum(source[idx]) || source[idx] == '_')) {
        idx++;
    }
    if (nameStart == idx) return nullptr;
    string name = source.substr(nameStart, idx - nameStart);

    while (idx < source.size() && isspace(source[idx])) idx++;
    string valStr = (idx < source.size()) ? source.substr(idx) : "0";
    
    int value = 0;
    try {
        value = stoi(valStr);
    } catch (...) {
        value = 0;
    }

    return make_shared<CompilerMacro>(move(name), value, pos.line, pos.column);
}

shared_ptr<TextMacro> MacroParser::parseTextMacro(
    const string& source, 
    ::Elica::Core::Position pos
) {
    size_t idx = 0;
    while (idx < source.size() && isspace(source[idx])) idx++;
    if (idx >= source.size() || source[idx] != '@') return nullptr;
    idx++;

    while (idx < source.size() && isspace(source[idx])) idx++;
    size_t nameStart = idx;
    while (idx < source.size() && (isalnum(source[idx]) || source[idx] == '_')) {
        idx++;
    }
    if (nameStart == idx) return nullptr;
    string name = source.substr(nameStart, idx - nameStart);

    vector<string> args;
    bool hasArgs = false;
    while (idx < source.size() && isspace(source[idx])) idx++;

    if (idx < source.size() && source[idx] == '(') {
        hasArgs = true;
        idx++;
        while (idx < source.size() && source[idx] != ')') {
            while (idx < source.size() && isspace(source[idx])) idx++;
            size_t argStart = idx;
            while (idx < source.size() && (isalnum(source[idx]) || source[idx] == '_')) {
                idx++;
            }
            if (argStart < idx) {
                args.push_back(source.substr(argStart, idx - argStart));
            }
            while (idx < source.size() && (isspace(source[idx]) || source[idx] == ',')) {
                idx++;
            }
        }
        if (idx < source.size() && source[idx] == ')') idx++;
    }

    while (idx < source.size() && isspace(source[idx])) idx++;
    string rawBody = (idx < source.size()) ? source.substr(idx) : "";

    while (!rawBody.empty() && isspace(rawBody.front())) rawBody.erase(rawBody.begin());
    while (!rawBody.empty() && isspace(rawBody.back())) rawBody.pop_back();

    if (rawBody.size() >= 2 && rawBody.front() == '{' && rawBody.back() == '}') {
        rawBody = rawBody.substr(1, rawBody.size() - 2);
        while (!rawBody.empty() && isspace(rawBody.front())) rawBody.erase(rawBody.begin());
        while (!rawBody.empty() && isspace(rawBody.back())) rawBody.pop_back();
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
    while (!flattenedBody.empty() && isspace(flattenedBody.front())) flattenedBody.erase(flattenedBody.begin());
    while (!flattenedBody.empty() && isspace(flattenedBody.back())) flattenedBody.pop_back();

    rawBody = flattenedBody;

    string hygienicBody = hasArgs ? applyHygiene(rawBody, args) : rawBody;

    if (hasArgs) {
        return make_shared<TextMacro>(move(name), move(hygienicBody), move(args), pos.line, pos.column);
    } else {
        return make_shared<TextMacro>(move(name), move(hygienicBody), pos.line, pos.column);
    }
}

shared_ptr<StructMacro> MacroParser::parseStructMacro(
    const string& source, 
    ::Elica::Core::Position pos
) {
    size_t idx = 0;
    while (idx < source.size() && isspace(source[idx])) idx++;
    if (idx >= source.size() || source[idx] != '&') return nullptr;
    idx++;

    while (idx < source.size() && isspace(source[idx])) idx++;
    size_t nameStart = idx;
    while (idx < source.size() && (isalnum(source[idx]) || source[idx] == '_')) {
        idx++;
    }
    if (nameStart == idx) return nullptr;
    string name = source.substr(nameStart, idx - nameStart);

    auto structMacro = make_shared<StructMacro>(name, pos);

    while (idx < source.size() && isspace(source[idx])) idx++;
    if (idx < source.size() && source[idx] == '(') {
        idx++;
        while (idx < source.size() && source[idx] != ')') {
            while (idx < source.size() && isspace(source[idx])) idx++;
            size_t pStart = idx;
            while (idx < source.size() && source[idx] != ',' && source[idx] != ')' && !isspace(source[idx])) {
                idx++;
            }
            if (pStart < idx) {
                structMacro->addPlaceholder(source.substr(pStart, idx - pStart));
            }
            while (idx < source.size() && (isspace(source[idx]) || source[idx] == ',')) {
                idx++;
            }
        }
        if (idx < source.size() && source[idx] == ')') idx++;
    }

    while (idx < source.size() && isspace(source[idx])) idx++;
    if (idx < source.size() && source[idx] == '=') {
        idx++;
        while (idx < source.size() && isspace(source[idx])) idx++;
        
        size_t targetStart = idx;
        while (idx < source.size() && source[idx] != '{' && source[idx] != ';' && !isspace(source[idx])) {
            idx++;
        }
        string targetType = source.substr(targetStart, idx - targetStart);
        
        if (targetType.rfind("operator", 0) == 0 && idx < source.size() && source[idx] == '(') {
            size_t opStart = source.find('"', idx);
            size_t opEnd = (opStart != string::npos) ? source.find('"', opStart + 1) : string::npos;
            if (opStart != string::npos && opEnd != string::npos) {
                targetType = "operator(" + source.substr(opStart + 1, opEnd - opStart - 1) + ")";
                idx = opEnd + 1;
                while (idx < source.size() && source[idx] != '{' && source[idx] != ';') idx++;
            }
        }
        structMacro->setTargetType(targetType);
    }

    while (idx < source.size() && isspace(source[idx])) idx++;
    if (idx < source.size() && source[idx] == '{') {
        idx++;
        while (idx < source.size() && source[idx] != '}') {
            while (idx < source.size() && isspace(source[idx])) idx++;
            
            size_t keyStart = idx;
            while (idx < source.size() && (isalnum(source[idx]) || source[idx] == '_')) idx++;
            string key = source.substr(keyStart, idx - keyStart);

            while (idx < source.size() && isspace(source[idx])) idx++;
            if (idx < source.size() && source[idx] == '=') {
                idx++;
                while (idx < source.size() && isspace(source[idx])) idx++;
                if (idx < source.size() && source[idx] == '{') {
                    idx++;
                    size_t quoteStart = idx;
                    int depth = 1;
                    while (idx < source.size() && depth > 0) {
                        if (source[idx] == '{') depth++;
                        else if (source[idx] == '}') depth--;
                        if (depth > 0) idx++;
                    }
                    string quoteBody = source.substr(quoteStart, idx - quoteStart);
                    if (idx < source.size() && source[idx] == '}') idx++;

                    if (key == "preQuotes") structMacro->setPreQuotes(quoteBody);
                    else if (key == "beginQuotes") structMacro->setBeginQuotes(quoteBody);
                    else if (key == "endQuotes") structMacro->setEndQuotes(quoteBody);
                    else if (key == "postQuotes") structMacro->setPostQuotes(quoteBody);
                }
            }

            while (idx < source.size() && (isspace(source[idx]) || source[idx] == ',' || source[idx] == ';')) {
                idx++;
            }
        }
    }

    return structMacro;
}

shared_ptr<Macro> MacroParser::parse(const string& source, ::Elica::Core::Position pos) {
    size_t idx = 0;
    while (idx < source.size() && isspace(source[idx])) idx++;
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