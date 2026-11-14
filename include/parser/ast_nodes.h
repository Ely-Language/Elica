// SPDX-License-Identifier: MIT
#pragma once

// AST for Ely, C and C++ — shared node set.
//
// The AST is built from Earley parse trees by the AstMaker. Nodes carry the
// source position of their first token so diagnostics can point at files.

#include <string>
#include <vector>
#include <memory>
#include <sstream>

#include "core/utils.h"

namespace Elica::Parsing {

struct Expr;
struct Stmt;
struct Decl;

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

struct TypeRef {
    std::vector<std::string> words;   // e.g. ["unsigned","long","long"] | ["std","::","string"]
    int pointerDepth = 0;             // '*' count
    bool isReference  = false;        // '&' (C++)
    bool isConst      = false;
    bool isElyFunc    = false;        // `... func name(...)` — Ely style

    bool empty() const { return words.empty() && pointerDepth == 0; }

    std::string toString() const {
        std::ostringstream os;
        if (isConst) os << "const ";
        for (size_t i = 0; i < words.size(); ++i) {
            if (i) os << ' ';
            os << words[i];
        }
        for (int i = 0; i < pointerDepth; ++i) os << '*';
        if (isReference) os << '&';
        return os.str();
    }
};

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

struct Expr {
    enum Kind {
        INT_LIT, FLT_LIT, STR_LIT, CHAR_LIT, BOOL_LIT,
        IDENT, THIS, NULLPTR,
        BINARY, UNARY, ASSIGN, TERNARY,
        CALL, INDEX, MEMBER, INC_DEC, NEW_EXPR, SIZEOF, NATIVE,
        RAW                                    // resilient fallback (C/C++)
    } kind;

    // literals / identifiers
    std::string text;

    // structure
    std::string op;
    std::shared_ptr<Expr> lhs, rhs, cond, thenBranch, elseBranch;
    std::shared_ptr<Expr> target, value;      // assign: target op= value
    std::shared_ptr<Expr> callee, base, index;
    std::vector<std::shared_ptr<Expr>> args;  // call args / new-args
    std::vector<std::string> members;          // member path (a.b.c)

    bool isPrefix = true;                      // ++/-- prefix or postfix
    bool isArrow  = false;                     // a->b vs a.b

    // new / sizeof
    TypeRef newType;                           // `new Foo(...)` / `sizeof(int)`
    bool newBrackets = false;                  // `new int[10]`
    std::shared_ptr<Expr> arraySize;           // `new int[N]`

    ::Elica::Core::Position pos;

    static std::shared_ptr<Expr> make(Expr::Kind k, std::string txt,
                                      ::Elica::Core::Position p) {
        auto e = std::make_shared<Expr>();
        e->kind = k; e->text = std::move(txt); e->pos = p;
        return e;
    }
};

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

struct Stmt {
    enum Kind {
        BLOCK, EXPR_STMT, VAR_DECL, IF, WHILE, FOR, RETURN,
        BREAK, CONTINUE, EMPTY, NATIVE_CODE,
        DO, SWITCH, GOTO, LABEL, RAW          // C/C++ resilient statements
    } kind;
    ::Elica::Core::Position pos;

    // block
    std::vector<std::shared_ptr<Stmt>> body;

    // expression statements / if conditions / while / return / for-head
    std::shared_ptr<Expr> expr;
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> thenBranch, elseBranch;
    std::shared_ptr<Stmt> init, incr;

    // var decl
    TypeRef type;
    std::string name;
    std::shared_ptr<Expr> initExpr;

    // native code (cppCode/cCode blocks) — raw text kept for later stages
    std::string nativeLang;   // "cpp" | "c"
    std::string nativeCode;

    // raw / label / goto
    std::string raw;
    std::string label;
};

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

struct Decl {
    enum Kind {
        FUNCTION, VARIABLE, CLASS, STRUCT, NAMESPACE, TYPEDEF, ENUM,
        USING, INCLUDE, TEMPLATE, EXTERN_BLOCK, ALIAS
    } kind;
    ::Elica::Core::Position pos;
    std::string name;

    // function
    TypeRef ret;
    std::vector<std::pair<TypeRef, std::string>> params;   // type + name
    std::vector<std::shared_ptr<Expr>> defaultArgs;
    std::vector<std::string> modifiers;                    // public/static/const/...
    std::shared_ptr<Stmt> body;                            // nullptr => declaration only
    bool isCtor = false, isDtor = false, isOperator = false;
    std::string operatorString;
    bool isElyFuncStyle = false;

    // class/struct
    bool isStruct = false;
    std::string baseName;                                  // `: public Base`
    std::vector<std::shared_ptr<Decl>> members;

    // using / include
    std::string modulePath;                                // using console; / #include <x>
    bool isAngleInclude = false;

    // template
    std::vector<std::string> templateParams;

    // typedef / alias
    TypeRef aliasType;

    // extern block / namespace
    std::vector<std::shared_ptr<Decl>> nested;
};

// ---------------------------------------------------------------------------
// Program = one parsed file
// ---------------------------------------------------------------------------

struct Program {
    std::string fileName;
    std::vector<std::shared_ptr<Decl>> decls;
    std::vector<std::string> includes;    // resolved #include/using info
};

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;
using DeclPtr = std::shared_ptr<Decl>;

} // namespace Elica::Parsing