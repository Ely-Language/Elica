#include "features/preprocessor.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

namespace Elica::Features::Preprocess {

Preprocessor::Preprocessor(Elica::Core::Project* proj)
    : project(proj), lexer("", Elica::Lexer::LexerLanguage::ELY) {}

void Preprocessor::replaceWithWhitespace(string& code, size_t start, size_t length) {
    for (size_t i = start; i < start + length && i < code.size(); ++i) {
        if (code[i] != '\n') {
            code[i] = ' ';
        }
    }
}

void Preprocessor::collectMacros(string& source, const fs::path& filePath) {
    size_t idx = 0;
    size_t currentLine = 1;

    while (idx < source.size()) {
        size_t lineStart = idx;
        while (idx < source.size() && isspace(source[idx])) {
            if (source[idx] == '\n') currentLine++;
            idx++;
        }

        if (idx >= source.size()) break;

        char prefix = source[idx];
        if (prefix == '#' || prefix == '@' || prefix == '&') {
            ::Elica::Core::Position pos(currentLine, 1);
            size_t macroStart = idx;

            int braceDepth = 0;
            bool inBlock = false;

            while (idx < source.size()) {
                char c = source[idx];
                
                if (c == '{') {
                    braceDepth++;
                    inBlock = true;
                } else if (c == '}') {
                    braceDepth--;
                }

                if (c == '\n') {
                    currentLine++;
                    
                    size_t checkIdx = idx;
                    if (checkIdx > 0 && source[checkIdx - 1] == '\r') checkIdx--;
                    bool hasBackslash = (checkIdx > macroStart && source[checkIdx - 1] == '\\');

                    if (!inBlock && !hasBackslash) {
                        idx++;
                        break;
                    }
                    if (inBlock && braceDepth == 0) {
                        idx++;
                        break;
                    }
                }
                idx++;
            }

            size_t macroEnd = idx;
            string macroStr = source.substr(macroStart, macroEnd - macroStart);
            
            string cleanMacroStr = macroStr;
            size_t bsPos;
            while ((bsPos = cleanMacroStr.find("\\\n")) != string::npos) {
                cleanMacroStr.erase(bsPos, 2);
            }
            while ((bsPos = cleanMacroStr.find("\\\r\n")) != string::npos) {
                cleanMacroStr.erase(bsPos, 3);
            }
            while (!cleanMacroStr.empty() && (cleanMacroStr.back() == '\\' || isspace(cleanMacroStr.back()))) {
                if (cleanMacroStr.back() == '\\') {
                    cleanMacroStr.pop_back();
                    break;
                }
                cleanMacroStr.pop_back();
            }

            auto parsedMacro = Macro::MacroParser::parse(cleanMacroStr, pos);

            if (parsedMacro) {
                if (prefix == '#') {
                    auto compMacro = dynamic_pointer_cast<Macro::CompilerMacro>(parsedMacro);
                    if (compMacro) project->compilerMacros.push_back(*compMacro);
                } else if (prefix == '@') {
                    auto textMacro = dynamic_pointer_cast<Macro::TextMacro>(parsedMacro);
                    if (textMacro) project->textMacros.push_back(*textMacro);
                } else if (prefix == '&') {
                    auto structMacro = dynamic_pointer_cast<Macro::StructMacro>(parsedMacro);
                    if (structMacro) project->structMacros.push_back(*structMacro);
                }
            }

            replaceWithWhitespace(source, macroStart, macroEnd - macroStart);

        } else {
            while (idx < source.size() && source[idx] != '\n') {
                idx++;
            }
            if (idx < source.size() && source[idx] == '\n') {
                currentLine++;
                idx++;
            }
        }
    }
}

string Preprocessor::expandTextMacros(const string& source, const fs::path file) {
    string processed = source;
    bool expandedAny = true;
    int iterations = 0;
    const int maxIterations = 100;

    cout << "Preprocessing file: " << file << "\n";

    while (expandedAny && iterations < maxIterations) {
        expandedAny = false;
        iterations++;

        for (const auto& macro : project->textMacros) {
            string name = macro.getName();
            size_t pos = 0;
            while ((pos = processed.find(name, pos)) != string::npos) {
                bool leftOk = (pos == 0 || (!isalnum(processed[pos - 1]) && processed[pos - 1] != '_'));
                bool rightOk = (pos + name.size() == processed.size() || (!isalnum(processed[pos + name.size()]) && processed[pos + name.size()] != '_'));

                if (leftOk && rightOk) {
                    if (macro.isFunctionlike()) {
                        size_t bracketPos = pos + name.size();
                        while (bracketPos < processed.size() && isspace(processed[bracketPos])) {
                            bracketPos++;
                        }
                        if (bracketPos < processed.size() && processed[bracketPos] == '(') {
                            size_t endBracket = bracketPos;
                            int depth = 0;
                            while (endBracket < processed.size()) {
                                if (processed[endBracket] == '(') depth++;
                                else if (processed[endBracket] == ')') {
                                    depth--;
                                    if (depth == 0) break;
                                }
                                endBracket++;
                            }

                            if (depth == 0) {
                                size_t totalLen = (endBracket + 1) - pos;
                                
                                string callArgsStr = processed.substr(bracketPos + 1, endBracket - (bracketPos + 1));
                                vector<string> callArgs;
                                {
                                    string curArg = "";
                                    int parenDepth = 0;
                                    int braceDepth = 0;
                                    int bracketDepth = 0;
                                    bool inString = false;
                                    char stringChar = '\0';
                                    bool escaped = false;

                                    for (size_t k = 0; k < callArgsStr.size(); ++k) {
                                        char ch = callArgsStr[k];

                                        if (inString) {
                                            if (escaped) {
                                                escaped = false;
                                            } else if (ch == '\\') {
                                                escaped = true;
                                            } else if (ch == stringChar) {
                                                inString = false;
                                            }
                                            curArg += ch;
                                        } else {
                                            if (ch == '"' || ch == '\'') {
                                                inString = true;
                                                stringChar = ch;
                                                curArg += ch;
                                            } else {
                                                if (ch == '(') parenDepth++;
                                                else if (ch == ')') parenDepth--;
                                                else if (ch == '{') braceDepth++;
                                                else if (ch == '}') braceDepth--;
                                                else if (ch == '[') bracketDepth++;
                                                else if (ch == ']') bracketDepth--;

                                                if (ch == ',' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0) {
                                                    while (!curArg.empty() && isspace(curArg.front())) curArg.erase(curArg.begin());
                                                    while (!curArg.empty() && isspace(curArg.back())) curArg.pop_back();
                                                    callArgs.push_back(curArg);
                                                    curArg = "";
                                                } else {
                                                    curArg += ch;
                                                }
                                            }
                                        }
                                    }
                                    while (!curArg.empty() && isspace(curArg.front())) curArg.erase(curArg.begin());
                                    while (!curArg.empty() && isspace(curArg.back())) curArg.pop_back();
                                    if (!curArg.empty()) callArgs.push_back(curArg);
                                }

                                string body = macro.getBody();
                                const auto& macroArgs = macro.getArgs();

                                for (size_t i = 0; i < macroArgs.size() && i < callArgs.size(); ++i) {
                                    string paramName = macroArgs[i];
                                    string argVal = callArgs[i];

                                    size_t pPos = 0;
                                    while ((pPos = body.find(paramName, pPos)) != string::npos) {
                                        bool lOk = (pPos == 0 || (!isalnum(body[pPos - 1]) && body[pPos - 1] != '_'));
                                        bool rOk = (pPos + paramName.size() == body.size() || (!isalnum(body[pPos + paramName.size()]) && body[pPos + paramName.size()] != '_'));
                                        if (lOk && rOk) {
                                            body.replace(pPos, paramName.size(), argVal);
                                            pPos += argVal.size();
                                        } else {
                                            pPos += paramName.size();
                                        }
                                    }
                                }

                                processed.replace(pos, totalLen, body);
                                expandedAny = true;
                                pos += body.size();
                                continue;
                            }
                        }
                    } else {
                        string body = macro.getBody();
                        processed.replace(pos, name.size(), body);
                        expandedAny = true;
#ifdef DEBUG
                        cout << "[DEBUG][EXPAND][Iter" << iterations << "]: Replaced macro '" << name 
                             << "' -> body: [" << body << "]\n";
#endif
                        pos += body.size();
                        continue;
                    }
                }
                pos += name.size();
            }
        }
    }
    return processed;
}

vector<Elica::Lexer::Token> Preprocessor::process(const fs::path& filePath, Elica::Lexer::LexerLanguage lang) {
    ifstream file(filePath);
    if (!file.is_open()) {
        if (project) {
            project->pushError(Elica::Core::Error(0, 0, filePath, "Failed to open file for preprocessing."));
        }
        return {};
    }

    string source((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    collectMacros(source, filePath);

    string cleanedSource = source;
    istringstream stream(cleanedSource);
    string lineStr;
    size_t charOffset = 0;

    while (getline(stream, lineStr)) {
        size_t idx = 0;
        while (idx < lineStr.size() && isspace(lineStr[idx])) {
            idx++;
        }
        if (idx < lineStr.size() && (lineStr[idx] == '#' || lineStr[idx] == '@' || lineStr[idx] == '&')) {
            replaceWithWhitespace(cleanedSource, charOffset, lineStr.size());
        }
        charOffset += lineStr.size() + 1;
    }

    string expandedSource = expandTextMacros(cleanedSource, filePath);

    lexer.setfull(expandedSource, lang);
    return lexer.tokenize();
}

}