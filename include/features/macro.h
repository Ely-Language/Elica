// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cctype>

#include "core/collector.h"
#include "core/utils.h"

using namespace std;

namespace Elica::Features::Preprocess {

namespace Macro {

class Macro {
protected:
    string name;
    ::Elica::Core::Position position;
public:
    Macro(string name, ::Elica::Core::Position pos) 
        : name(move(name)), position(pos) {}
    virtual ~Macro() = default;
    string getName() const { return this->name; }
    ::Elica::Core::Position getPosition() const { return this->position; }
};
// #
class CompilerMacro : public Macro {
private:
    int value;
public:
    CompilerMacro(
        string macroName,
        int macroValue,
        size_t line,
        size_t column
    ) : Macro(move(macroName), ::Elica::Core::Position(line, column)), 
        value(macroValue) {}
    
    int getValue() const { return this->value; }
};
// @
class TextMacro : public Macro {
private:
    string body;
    vector<string> args;
    bool isFunctionLike;
public:
    TextMacro(
        string macroName, 
        string bodyText,
        size_t line,
        size_t column
    ) : Macro(move(macroName), ::Elica::Core::Position(line, column)),
        body(bodyText), isFunctionLike(false) {}

    TextMacro(
        string macroName, 
        string bodyText,
        vector<string> argsList,
        size_t line,
        size_t column
    ) : Macro(move(macroName), ::Elica::Core::Position(line, column)),
        body(bodyText), isFunctionLike(true), args(argsList) {}

    const string& getBody() const { return this->body; }
    const vector<string>& getArgs() const { return this->args; }
    bool isFunctionlike() const { return this->isFunctionLike; }
};

// &
class StructMacro : public Macro {
private:
    vector<string> placeholders;
    string targetType;

    string preQuotes;
    string beginQuotes;
    string endQuotes;
    string postQuotes;

    string declaredName;        
    string typeOrParentClass;   

    unordered_map<string, string> fields;                       
    unordered_map<string, string> waited;                       
    unordered_map<string, string> unwaited;                     
    unordered_map<string, unordered_map<string, string>> methods; 

    unordered_map<string, string> args;                         

public:
    StructMacro(string macroName, ::Elica::Core::Position pos)
        : Macro(move(macroName), pos) {}

    const vector<string>& getPlaceholders() const { return placeholders; }
    const string& getTargetType() const { return targetType; }

    const string& getPreQuotes() const { return preQuotes; }
    const string& getBeginQuotes() const { return beginQuotes; }
    const string& getEndQuotes() const { return endQuotes; }
    const string& getPostQuotes() const { return postQuotes; }
    const string& getDeclaredName() const { return declaredName; }
    const string& getTypeOrParentClass() const { return typeOrParentClass; }

    const unordered_map<string, string>& getFields() const { return fields; }
    const unordered_map<string, string>& getWaited() const { return waited; }
    const unordered_map<string, string>& getUnwaited() const { return unwaited; }
    const unordered_map<string, unordered_map<string, string>>& getMethods() const { return methods; }
    const unordered_map<string, string>& getArgs() const { return args; }
    
    void addPlaceholder(string placeholder) { 
        placeholders.push_back(move(placeholder)); 
    }

    void setTargetType(string type) {
        targetType = move(type);
    }
    
    void setQuotes(string pre, string begin, string end, string post) {
        preQuotes = move(pre);
        beginQuotes = move(begin);
        endQuotes = move(end);
        postQuotes = move(post);
    }

    void setPreQuotes(string quotes) { preQuotes = move(quotes); }
    void setBeginQuotes(string quotes) { beginQuotes = move(quotes); }
    void setEndQuotes(string quotes) { endQuotes = move(quotes); }
    void setPostQuotes(string quotes) { postQuotes = move(quotes); }

    void setDeclaredName(string name) { declaredName = move(name); }
    void setTypeOrParentClass(string val) { typeOrParentClass = move(val); }

    void addField(string name, string type) { fields[move(name)] = move(type); }
    void addWaitedField(string name, string type) { waited[move(name)] = move(type); }
    void addUnwaitedField(string name, string type) { unwaited[move(name)] = move(type); }
    void addMethod(string methodName, unordered_map<string, string> methodArgs) {
        methods[move(methodName)] = move(methodArgs);
    }
    void addArg(string name, string type) { args[move(name)] = move(type); }
};

class MacroParser {
private:
    static string applyHygiene(
        const string& body, 
        const vector<string>& preserverNames
    );

    static shared_ptr<CompilerMacro> parseCompilerMacro(
        const string& source, 
        ::Elica::Core::Position pos
    );
    
    static shared_ptr<TextMacro> parseTextMacro(
        const string& source, 
        ::Elica::Core::Position pos
    );

    static shared_ptr<StructMacro> parseStructMacro(
        const string& source, 
        ::Elica::Core::Position pos
    );

public:
    static shared_ptr<Macro> parse(const string& source, ::Elica::Core::Position pos);
};

}

}