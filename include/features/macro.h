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

namespace Elica::Features::Preprocess {

namespace Macro {

class Macro {
protected:
    std::string name;
    ::Elica::Core::Position position;
public:
    Macro(std::string name, ::Elica::Core::Position pos)
        : name(std::move(name)), position(pos) {}
    virtual ~Macro() = default;
    std::string getName() const { return this->name; }
    ::Elica::Core::Position getPosition() const { return this->position; }
};

// # ---------------------------------------------------------------
class CompilerMacro : public Macro {
private:
    int value;
public:
    CompilerMacro(std::string macroName, int macroValue, size_t line, size_t column)
        : Macro(std::move(macroName), ::Elica::Core::Position(line, column)),
          value(macroValue) {}
    int getValue() const { return this->value; }
};

// @ ---------------------------------------------------------------
class TextMacro : public Macro {
private:
    std::string body;
    std::vector<std::string> args;
    bool isFunctionLike;
    bool isOperator = false;
    std::string opString;

public:
    TextMacro(std::string macroName, std::string bodyText, size_t line, size_t column)
        : Macro(move(macroName), ::Elica::Core::Position(line, column)),
          body(bodyText), isFunctionLike(false) {}

    TextMacro(std::string macroName, std::string bodyText, std::vector<std::string> argsList,
              size_t line, size_t column)
        : Macro(move(macroName), ::Elica::Core::Position(line, column)),
          body(bodyText), isFunctionLike(true), args(move(argsList)) {}

    const std::string &getBody() const { return this->body; }
    const std::vector<std::string> &getArgs() const { return this->args; }
    bool isFunctionlike() const { return this->isFunctionLike; }

    bool isOperatorMacro() const { return this->isOperator; }
    const std::string &getOpString() const { return this->opString; }
    void setOperator(std::string op) { isOperator = true; opString = std::move(op); }
};

//  &---------------------------------------------------------------
class StructMacro : public Macro {
private:
    std::vector<std::string> placeholders;
    std::string targetType;

    std::string preQuotes, beginQuotes, endQuotes, postQuotes;

    std::string declaredName;
    std::string typeOrParentClass;

    // B1: ordered, чтобы сохранить порядок объявления для F3OPL
    ::Elica::Core::OrderedMap<std::string> fields;
    ::Elica::Core::OrderedMap<std::string> waited;
    ::Elica::Core::OrderedMap<std::string> unwaited;
    ::Elica::Core::OrderedMap<::Elica::Core::OrderedMap<std::string>> methods;
    ::Elica::Core::OrderedMap<std::string> args;

public:
    StructMacro(std::string macroName, ::Elica::Core::Position pos)
        : Macro(std::move(macroName), pos) {}

    const std::vector<std::string> &getPlaceholders() const { return placeholders; }
    const std::string &getTargetType()     const { return targetType;     }
    const std::string &getPreQuotes()      const { return preQuotes;      }
    const std::string &getBeginQuotes()    const { return beginQuotes;    }
    const std::string &getEndQuotes()      const { return endQuotes;      }
    const std::string &getPostQuotes()     const { return postQuotes;     }
    const std::string &getDeclaredName()   const { return declaredName;   }
    const std::string &getTypeOrParentClass() const { return typeOrParentClass; }

    const ::Elica::Core::OrderedMap<std::string> &getFields()    const { return fields;    }
    const ::Elica::Core::OrderedMap<std::string> &getWaited()    const { return waited;    }
    const ::Elica::Core::OrderedMap<std::string> &getUnwaited()  const { return unwaited;  }
    const ::Elica::Core::OrderedMap<::Elica::Core::OrderedMap<std::string>> &getMethods() const { return methods; }
    const ::Elica::Core::OrderedMap<std::string> &getArgs()      const { return args;      }

    void addPlaceholder(std::string placeholder) { placeholders.push_back(std::move(placeholder)); }
    void setTargetType(std::string type)         { targetType = std::move(type); }
    void setQuotes(std::string pre, std::string begin, std::string end, std::string post) {
        preQuotes = std::move(pre); beginQuotes = std::move(begin);
        endQuotes = std::move(end); postQuotes  = std::move(post);
    }
    void setPreQuotes(std::string q)   { preQuotes   = std::move(q); }
    void setBeginQuotes(std::string q) { beginQuotes = std::move(q); }
    void setEndQuotes(std::string q)   { endQuotes   = std::move(q); }
    void setPostQuotes(std::string q)  { postQuotes  = std::move(q); }

    void setDeclaredName(std::string n)       { declaredName = std::move(n); }
    void setTypeOrParentClass(std::string v)  { typeOrParentClass = std::move(v); }

    void addField(std::string n, std::string t)          { fields[std::move(n)]    = std::move(t); }
    void addWaitedField(std::string n, std::string t)    { waited[std::move(n)]    = std::move(t); }
    void addUnwaitedField(std::string n, std::string t)  { unwaited[std::move(n)]  = std::move(t); }
    void addMethod(std::string m, ::Elica::Core::OrderedMap<std::string> a) { methods[std::move(m)] = std::move(a); }
    void addArg(std::string n, std::string t)            { args[std::move(n)]      = std::move(t); }
};

class MacroParser {
private:
    static std::string applyHygiene(const std::string &body, const std::vector<std::string> &preserverNames);
    static std::shared_ptr<CompilerMacro> parseCompilerMacro(const std::string &source, ::Elica::Core::Position pos);
    static std::shared_ptr<TextMacro>     parseTextMacro   (const std::string &source, ::Elica::Core::Position pos);
    static std::shared_ptr<StructMacro>   parseStructMacro (const std::string &source, ::Elica::Core::Position pos);

public:
    static std::shared_ptr<Macro> parse(const std::string &source, ::Elica::Core::Position pos);
};

}
}