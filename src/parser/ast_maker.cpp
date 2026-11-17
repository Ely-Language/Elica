// SPDX-License-Identifier: MIT
#include "parser/ast_maker.h"

#include <iostream>
#include <sstream>

#include "lexer/token.h"
#include "parser/grammars.h"

namespace Elica::Parsing {

using Earley::ParseNode;
namespace Lex = ::Elica::Lexer;

namespace {

struct Ctx {
    const std::vector<Lex::Token> &tokens;
    const ParseNode *child(const ParseNode &n, size_t i) const {
        return (i < n.children.size()) ? n.children[i].get() : nullptr;
    }
    const ParseNode *expect(const ParseNode &n, size_t i) const {
        const ParseNode *c = child(n, i);
        if (!c) throw std::runtime_error("AST: missing child " + std::to_string(i) + " of " + n.symbol);
        return c;
    }
};

::Elica::Core::Position posOf(const ParseNode &n, const Ctx &) {
    if (n.tokenIndex >= 0) return {}; // leaf: assigned in makeX via leafContext
    return {};
}

std::string leafText(const ParseNode &n, const Ctx &ctx) {
    if (n.tokenIndex >= 0 && n.tokenIndex < static_cast<int>(ctx.tokens.size()))
        return ctx.tokens[static_cast<size_t>(n.tokenIndex)].lexeme;
    return n.symbol;
}

// Descend single-child nonterminal chains to the terminal and return its text.
std::string opText(const ParseNode &n) {
    const ParseNode *cur = &n;
    while (cur->children.size() == 1 && !cur->children[0]->children.empty()) {
        cur = cur->children[0].get();
    }
    if (cur->children.size() == 1) return cur->children[0]->symbol;
    return cur->symbol;
}

// Text of a terminal leaf (possibly under single-child chains).
std::string terminalLeafText(const ParseNode &n, const Ctx &ctx) {
    const ParseNode *cur = &n;
    while (true) {
        if (cur->children.size() == 1) {
            const ParseNode *ch = cur->children[0].get();
            cur = ch;                               // descend
            if (ch->children.empty() && ch->tokenIndex >= 0) break; // terminal found
            continue;
        }
        break;
    }
    return leafText(*cur, ctx);
}

TypeRef toType(const ParseNode &n, const Ctx &ctx);

std::string collectTemplateArgs(const ParseNode &n, const Ctx &ctx) {
    std::string out;
    auto walk = [&](auto &&self, const ParseNode &node) -> void {
        if (node.children.empty()) return;
        if (node.children.size() == 1) { self(self, *node.children[0]); return; }
        if (node.children.size() == 3) {
            self(self, *node.children[0]);
            self(self, *node.children[2]);
            return;
        }
        // template_arg leaf: type | num
        if (node.symbol == "template_arg" && !node.children.empty()) {
            if (!out.empty()) out += ", ";
            const ParseNode &inner = *node.children[0];
            if (inner.symbol == "type") {
                out += toType(inner, ctx).toString();
            } else if (inner.symbol == "num") {
                out += leafText(inner, ctx);
            }
        }
    };
    walk(walk, n);
    return out;
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

std::string idPathText(const ParseNode &n, const Ctx &ctx) {
    if (n.children.empty()) return leafText(n, ctx);
    if (n.children.size() == 1) return idPathText(*n.children[0], ctx);
    if (n.children.size() < 3) {
        std::string s = idPathText(*n.children[0], ctx);
        return s;
    }
    // [id_path, ('.'|'::'), id]
    std::string lhs = idPathText(*n.children[0], ctx);
    std::string sep = n.children[1]->symbol;
    return lhs + sep + leafText(*n.children[2], ctx);
}

void collectTypeWord(TypeRef &t, const ParseNode &n, const Ctx &ctx) {
    if (n.children.empty()) { t.words.push_back(leafText(n, ctx)); return; }
    const std::string &s0 = n.children[0]->symbol;
    
    if (s0 == ".") {
        t.words.push_back(".");
        if (n.children.size() > 1) t.words.push_back(idPathText(*n.children[1], ctx));
    } else if (s0 == "::") {
        t.words.push_back("::");
        if (n.children.size() > 1) t.words.push_back(idPathText(*n.children[1], ctx));
    } else if (s0 == "type_kw") {
        t.words.push_back(terminalLeafText(*n.children[0], ctx));
    } else if (s0 == "id_path") {
        if (n.children.size() >= 4 && n.children[1]->symbol == "<") {
            std::string path = idPathText(*n.children[0], ctx);
            std::string args = collectTemplateArgs(*n.children[2], ctx);
            t.words.push_back(path + "<" + args + ">");
        } else {
            t.words.push_back(idPathText(*n.children[0], ctx));
        }
    } else if (s0 == "const") {
        t.isConst = true;
    } else if (s0 == "volatile") {
        t.words.push_back("volatile");
    } else if (s0 == "struct" || s0 == "enum" || s0 == "class" || s0 == "typename") {
        t.words.push_back(s0);
        t.words.push_back(idPathText(*n.children[1], ctx));
    } else {
        t.words.push_back(leafText(*n.children[0], ctx));
    }
}

void collectTypeTail(TypeRef &t, const ParseNode &n, const Ctx &ctx) {
    if (n.children.empty()) return;
    collectTypeWord(t, *n.children[0], ctx);
    if (n.children.size() == 2) collectTypeTail(t, *n.children[1], ctx);
}

void collectPtrTail(TypeRef &t, const ParseNode &n, const Ctx &ctx) {
    if (n.children.empty()) return;
    const std::string &s0 = n.children[0]->symbol;
    if (s0 == "*") { t.pointerDepth++; collectPtrTail(t, *n.children[1], ctx); }
    else if (s0 == "&" || s0 == "&&") { t.isReference = true; collectPtrTail(t, *n.children[1], ctx); }
}

TypeRef toType(const ParseNode &n, const Ctx &ctx) {
    TypeRef t;
    if (n.children.size() >= 1) collectTypeTail(t, *n.children[0], ctx);
    if (n.children.size() >= 2) collectPtrTail(t, *n.children[1], ctx);
    return t;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

std::vector<ExprPtr> interpParts(const std::string &raw, Lex::LexerLanguage lang);
ExprPtr toExpr(const ParseNode &n, const Ctx &ctx);

ExprPtr makeLeaf(Expr::Kind k, const ParseNode &n, const Ctx &ctx) {
    auto e = Expr::make(k, leafText(n, ctx), ::Elica::Core::Position());
    return e;
}

std::vector<ExprPtr> toArgs(const ParseNode &n, const Ctx &ctx) {
    std::vector<ExprPtr> out;
    if (n.children.empty()) return out;                    // args -> ε
    const ParseNode &list = *n.children[0];                // args_list
    // args_list -> args_list ',' expr | expr
    if (list.children.size() == 1) {
        out.push_back(toExpr(*list.children[0], ctx));
    } else if (list.children.size() == 3) {
        auto prev = toArgs(list, ctx);
        out = std::move(prev);
        out.push_back(toExpr(*list.children[2], ctx));
    }
    return out;
}

ExprPtr toExpr(const ParseNode &n, const Ctx &ctx) {
    const std::string &s = n.symbol;

    // ------- terminal leaves -------------------------------------------------
    if (n.children.empty()) {
        if (s == "id") return makeLeaf(Expr::IDENT, n, ctx);
        if (s == "num") {
            bool isFlt = false;
            if (n.tokenIndex >= 0 && n.tokenIndex < static_cast<int>(ctx.tokens.size())) {
                const auto &t = ctx.tokens[static_cast<size_t>(n.tokenIndex)];
                if (std::holds_alternative<Lex::CommonToken>(t.type)
                    && std::get<Lex::CommonToken>(t.type) == Lex::CommonToken::FLT_LITERAL) isFlt = true;
            }
            return makeLeaf(isFlt ? Expr::FLT_LIT : Expr::INT_LIT, n, ctx);
        }
        if (s == "str_lit") {
            if (n.tokenIndex >= 0 && n.tokenIndex < static_cast<int>(ctx.tokens.size())) {
                const auto &t = ctx.tokens[static_cast<size_t>(n.tokenIndex)];
                bool isF = false;
                if (std::holds_alternative<Lex::CommonToken>(t.type)) {
                    const auto ct = std::get<Lex::CommonToken>(t.type);
                    isF = (ct == Lex::CommonToken::F_STR_LITERAL
                        || ct == Lex::CommonToken::F_MULTILINE_STR_LITERAL);
                }
                if (isF) {
                    auto e = Expr::make(Expr::INTERP_STR, t.lexeme, t.position);
                    e->parts = interpParts(t.lexeme, Lex::LexerLanguage::ELY);
                    return e;
                }
            }
            return makeLeaf(Expr::STR_LIT, n, ctx);
        }
        if (s == "char_lit") return makeLeaf(Expr::CHAR_LIT, n, ctx);
        if (s == "true" || s == "false") return makeLeaf(Expr::BOOL_LIT, n, ctx);
        if (s == "this") return makeLeaf(Expr::THIS, n, ctx);
        if (s == "nullptr") return makeLeaf(Expr::NULLPTR, n, ctx);
        if (s == "cpp_code") return makeLeaf(Expr::NATIVE, n, ctx);
        if (s == "arg_item") {
        if (n.children.size() == 1)
            return toExpr(*n.children[0], ctx);
            auto e = Expr::make(Expr::ASSIGN, "named=", ::Elica::Core::Position());
            e->target = makeLeaf(Expr::IDENT, *n.children[0], ctx);
            e->value  = toExpr(*n.children[2], ctx);
            return e;
        }
        return makeLeaf(Expr::IDENT, n, ctx);
    }

    const Ctx &C = ctx;

    // ------- wrapper / composite rules ---------------------------------------
    if (s == "expr") return toExpr(*C.expect(n, 0), ctx);

    if (s == "arg_item") {
        if (n.children.size() == 1) return toExpr(*n.children[0], ctx);   // позиционный
        // P1.4: arg_item -> id '=' expr
        auto e = Expr::make(Expr::ASSIGN, "named", ::Elica::Core::Position());
        e->target = makeLeaf(Expr::IDENT, *n.children[0], ctx);
        e->value  = toExpr  (*n.children[2], ctx);
        return e;
    }
    if (s == "assign_expr") {
        if (n.children.size() == 1) return toExpr(*n.children[0], ctx);
        auto e = Expr::make(Expr::ASSIGN, opText(*n.children[1]), ::Elica::Core::Position());
        e->target = toExpr(*n.children[0], ctx);
        e->value  = toExpr(*n.children[2], ctx);
        return e;
    }
    if (s == "ternary") {
        if (n.children.size() == 1) return toExpr(*n.children[0], ctx);
        auto e = Expr::make(Expr::TERNARY, "?:", ::Elica::Core::Position());
        e->cond       = toExpr(*n.children[0], ctx);
        e->thenBranch = toExpr(*n.children[2], ctx);
        e->elseBranch = toExpr(*n.children[4], ctx);
        return e;
    }
    if (s == "logical_or" || s == "logical_and" || s == "bit_or" || s == "bit_xor"
        || s == "bit_and" || s == "equality" || s == "relational" || s == "shift"
        || s == "additive" || s == "multiplicative") {
        if (n.children.size() == 1) return toExpr(*n.children[0], ctx);
        auto e = Expr::make(Expr::BINARY, opText(*n.children[1]), ::Elica::Core::Position());
        e->lhs = toExpr(*n.children[0], ctx);
        e->rhs = toExpr(*n.children[2], ctx);
        return e;
    }
    if (s == "unary") {
        if (n.children.size() == 2) {
            auto e = Expr::make(Expr::UNARY, opText(*n.children[0]), ::Elica::Core::Position());
            e->lhs = toExpr(*n.children[1], ctx);
            return e;
        }
        return toExpr(*n.children[0], ctx);          // new_expr | sizeof_expr | delete_expr | postfix
    }
    if (s == "delete_expr") {                        // delete X | delete[] X
        auto e = Expr::make(Expr::UNARY, "delete", ::Elica::Core::Position());
        const ParseNode *operand = (n.children.size() == 2) ? n.children[1].get()
                                                            : n.children[3].get();
        e->lhs = toExpr(*operand, ctx);
        e->newBrackets = (n.children.size() == 4);   // delete[]
        return e;
    }
    if (s == "new_expr") {
        auto e = Expr::make(Expr::NEW_EXPR, "new", ::Elica::Core::Position());
        const ParseNode &body = *C.expect(n, 1);     // new_body
        // P0.4: `new Foo(args)` / `new a.b.Ctor(args)` via id_path
        if (body.children[0]->symbol == "id_path") {
            TypeRef t;
            t.words.push_back(idPathText(*body.children[0], ctx));
            e->newType = std::move(t);
            if (body.children.size() >= 4) e->args = toArgs(*body.children[2], ctx);
            return e;
        }
        if (body.children[0]->symbol == "type") {
            e->newType = toType(*body.children[0], ctx);
            const ParseNode *arr = C.child(body, 1);
            if (arr && arr->children.size() == 3) { e->newBrackets = true; e->arraySize = toExpr(*arr->children[1], ctx); }
            else if (arr && arr->children.size() == 2) { e->newBrackets = true; }
        } else if (body.children[0]->symbol == "id_path") {
            // new id_path(args) — constructor call with qualified name
            e->target = Expr::make(Expr::IDENT, idPathText(*body.children[0], ctx), ::Elica::Core::Position());
            if (body.children.size() == 4) {
                e->args = toArgs(*body.children[2], ctx);
            }
        } else {
            e->target = toExpr(*body.children[0], ctx);
            if (body.children.size() == 4) {           // primary ( args )
                e->args = toArgs(*body.children[2], ctx);
            }
        }
        return e;
    }
    if (s == "new_body") {
        // reached only via new_expr -> handled there; direct fallthrough
        return toExpr(*n.children[0], ctx);
    }
    if (s == "sizeof_expr") {
        auto e = Expr::make(Expr::SIZEOF, "sizeof", ::Elica::Core::Position());
        if (n.children.size() == 4) { e->newType = toType(*n.children[2], ctx); }
        else { e->lhs = toExpr(*n.children[1], ctx); }
        return e;
    }
    if (s == "postfix") {
        if (n.children.size() == 1) return toExpr(*n.children[0], ctx);
        const std::string &op = n.children[1]->symbol;
        if (n.children.size() == 7) {                      // <template> (args)
            auto e = Expr::make(Expr::CALL, "call", ::Elica::Core::Position());
            e->callee = toExpr(*n.children[0], ctx);
            e->args   = toArgs(*n.children[5], ctx);
            return e;
        }
        if (n.children.size() == 4 && op == "(") {         // call
            auto e = Expr::make(Expr::CALL, "call", ::Elica::Core::Position());
            e->callee = toExpr(*n.children[0], ctx);
            e->args   = toArgs(*n.children[2], ctx);
            return e;
        }
        if (n.children.size() == 4 && op == "[") {         // index
            auto e = Expr::make(Expr::INDEX, "[]", ::Elica::Core::Position());
            e->base  = toExpr(*n.children[0], ctx);
            e->index = toExpr(*n.children[2], ctx);
            return e;
        }
        if (n.children.size() == 4 && op == "?") {          // postfix ? . id
            auto e = Expr::make(Expr::MEMBER, "?.", ::Elica::Core::Position());
            e->base      = toExpr(*n.children[0], ctx);
            e->isSafeNav = true;
            e->members.push_back(leafText(*n.children[3], ctx));
            return e;
        }
        if (n.children.size() == 3 && (op == "." || op == "->" || op == "::")) {
            auto e = Expr::make(Expr::MEMBER, op, ::Elica::Core::Position());
            e->base = toExpr(*n.children[0], ctx);
            e->isArrow = (op == "->");
            e->members.push_back(leafText(*n.children[2], ctx));
            return e;
        }
        if (n.children.size() == 2 && (op == "++" || op == "--")) {
            auto e = Expr::make(Expr::INC_DEC, op, ::Elica::Core::Position());
            e->lhs = toExpr(*n.children[0], ctx);
            e->isPrefix = false;
            return e;
        }
        if (n.children.size() == 7) {
            auto e = Expr::make(Expr::CALL, "call", ::Elica::Core::Position());
            auto callee = toExpr(*n.children[0], ctx);
            if (callee->kind == Expr::IDENT) {
                std::string targs = collectTemplateArgs(*n.children[2], ctx);
                callee->text = callee->text + "<" + targs + ">";
            }
            e->callee = callee;
            e->args = toArgs(*n.children[5], ctx);
            return e;
        }
        return toExpr(*n.children[0], ctx);
    }
    if (s == "primary") {
        if (n.children.size() == 3) return toExpr(*n.children[1], ctx);   // ( expr )
        
        // ::id_path — absolute scope resolution in C/C++ (e.g. ::std::string)
            if (n.children.size() == 2 && n.children[0]->symbol == "::") {
            std::string path = idPathText(*n.children[1], ctx);
            return Expr::make(Expr::IDENT, "::" + path, ::Elica::Core::Position());
        }
        
        // .id_path — relative path in Ely (e.g. .str, .__YS.ystr)
        if (n.children.size() == 2 && n.children[0]->symbol == ".") {
            std::string path = idPathText(*n.children[1], ctx);
            return Expr::make(Expr::IDENT, "." + path, ::Elica::Core::Position());
        }
        
        if (n.children.size() == 7 && n.children[1]->symbol == "<") {
            // cast like static_cast<int>(x)
            auto e = Expr::make(Expr::CALL, "cast", ::Elica::Core::Position());
            e->callee = Expr::make(Expr::IDENT, leafText(*n.children[0], ctx), ::Elica::Core::Position());
            e->target = std::make_shared<Expr>();
            e->target->kind = Expr::RAW;
            e->target->text = toType(*n.children[2], ctx).toString();
            e->args.push_back(toExpr(*n.children[5], ctx));
            return e;
        }
        return toExpr(*n.children[0], ctx);
    }
    if (s == "lambda_expr") {                 // [caps](params) -> ret { block }
        auto e = Expr::make(Expr::RAW, "<lambda>", ::Elica::Core::Position());
        return e;
    }
    // args / args_list handled by toArgs
    return makeLeaf(Expr::IDENT, n, ctx);
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

std::vector<StmtPtr> toStmts(const ParseNode &n, const Ctx &ctx);

StmtPtr makeStmt(Stmt::Kind k, const ParseNode &n) {
    auto s = std::make_shared<Stmt>();
    s->kind = k;
    return s;
}

StmtPtr wrapBlock(std::vector<StmtPtr> &&list) {
    if (list.size() == 1) return list[0];
    auto b = std::make_shared<Stmt>();
    b->kind = Stmt::BLOCK;
    b->body = std::move(list);
    return b;
}

struct Declarator {
    TypeRef type;
    std::string name;
    ExprPtr init;
    bool hasArr = false;
};

// extracts (type, declarators..) from a var_decl / c_field parse node
std::vector<Declarator> parseVarDecl(const ParseNode &n, const Ctx &ctx) {
    // var_decl -> type init_declarator decl_rest ';'
    std::vector<Declarator> out;
    TypeRef base = toType(*ctx.expect(n, 0), ctx);

    auto parseDeclarator = [&](const ParseNode &d) {
        // init_declarator -> ptrs id arr_suffix init_part
        Declarator dl;
        dl.type = base;
        const ParseNode *ptrs = ctx.child(d, 0);
        if (ptrs) {
            for (const auto &child : ptrs->children) {
                if (child->symbol == "*") dl.type.pointerDepth++;
                else if (child->symbol == "&") dl.type.isReference = true;
            }
        }
        const ParseNode *idNode = ctx.child(d, 1);
        dl.name = idNode ? leafText(*idNode, ctx) : "?";
        const ParseNode *arr = ctx.child(d, 2);
        if (arr && !arr->children.empty()) dl.hasArr = true;
        const ParseNode *init = ctx.child(d, 3);
        if (init && !init->children.empty()) {
            if (init->children[0]->symbol == "=") {
                dl.init = toExpr(*init->children[1], ctx);
            } else { // '(' args ')' — call-style init (C++)
                auto call = Expr::make(Expr::CALL, "call", ::Elica::Core::Position());
                call->callee = Expr::make(Expr::IDENT, dl.name, ::Elica::Core::Position());
                call->args = toArgs(*init->children[1], ctx);
                dl.init = call;
            }
        }
        out.push_back(std::move(dl));
    };

    parseDeclarator(*ctx.expect(n, 1));
    const ParseNode *rest = ctx.child(n, 2);
    if (rest) {
        for (const auto &ch : rest->children) {
            if (ch->symbol == "init_declarator") parseDeclarator(*ch);
        }
    }
    return out;
}

std::vector<StmtPtr> toStmts(const ParseNode &n, const Ctx &ctx) {
    const std::string &s = n.symbol;
    if (s == "stmt") return toStmts(*ctx.expect(n, 0), ctx);

    if (s == "expr_stmt") {
        auto st = makeStmt(Stmt::EXPR_STMT, n);
        st->expr = toExpr(*ctx.expect(n, 0), ctx);
        return {st};
    }
    if (s == "block") {
        auto st = makeStmt(Stmt::BLOCK, n);
        st->body = toStmts(*ctx.expect(n, 1), ctx);   // stmt_list
        return {st};
    }
    if (s == "stmt_list") {
        std::vector<StmtPtr> out;
        if (n.children.size() >= 2) {
            auto prev = toStmts(*n.children[0], ctx);
            auto tail = toStmts(*n.children[1], ctx);
            out.reserve(prev.size() + tail.size());
            out.insert(out.end(), prev.begin(), prev.end());
            out.insert(out.end(), tail.begin(), tail.end());
        }
        return out;
    }
    if (s == "var_decl") {
        std::vector<StmtPtr> out;
        for (auto &d : parseVarDecl(n, ctx)) {
            auto st = makeStmt(Stmt::VAR_DECL, n);
            st->type = d.type;
            st->name = d.name;
            st->initExpr = d.init;
            out.push_back(st);
        }
        return out;
    }
    if (s == "if_stmt") {
        auto st = makeStmt(Stmt::IF, n);
        st->cond = toExpr(*ctx.expect(n, 2), ctx);
        st->thenBranch = wrapBlock(toStmts(*ctx.expect(n, 4), ctx));
        if (n.children.size() == 7) st->elseBranch = wrapBlock(toStmts(*ctx.expect(n, 6), ctx));
        return {st};
    }
    if (s == "while_stmt") {
        auto st = makeStmt(Stmt::WHILE, n);
        st->cond = toExpr(*ctx.expect(n, 2), ctx);
        st->body.push_back(wrapBlock(toStmts(*ctx.expect(n, 4), ctx)));
        return {st};
    }
    if (s == "do_stmt") {                     // do stmt while ( expr ) ;
        auto st = makeStmt(Stmt::DO, n);
        st->body.push_back(wrapBlock(toStmts(*ctx.expect(n, 1), ctx)));
        st->cond = toExpr(*ctx.expect(n, 4), ctx);
        return {st};
    }
    if (s == "switch_stmt") {                 // switch ( expr ) stmt
        auto st = makeStmt(Stmt::SWITCH, n);
        st->cond = toExpr(*ctx.expect(n, 2), ctx);
        st->body.push_back(wrapBlock(toStmts(*ctx.expect(n, 4), ctx)));
        return {st};
    }
    if (s == "case_label") {                  // case expr : stmt | default : stmt
        auto st = makeStmt(Stmt::LABEL, n);
        if (n.children[0]->symbol == "default") st->label = "default";
        else st->label = "case:";
        auto tail = toStmts(*ctx.expect(n, n.children.size() - 1), ctx);
        st->body = tail;
        return {st};
    }
    if (s == "goto_stmt") {
        auto st = makeStmt(Stmt::GOTO, n);
        st->label = leafText(*ctx.expect(n, 1), ctx);
        return {st};
    }
    if (s == "label_stmt") {                  // id : stmt
        auto st = makeStmt(Stmt::LABEL, n);
        st->label = leafText(*ctx.expect(n, 0), ctx);
        st->body = toStmts(*ctx.expect(n, 2), ctx);
        return {st};
    }
    if (s == "try_stmt") {                    // try block catch_clause_list
        auto st = makeStmt(Stmt::BLOCK, n);
        st->body.push_back(wrapBlock(toStmts(*ctx.expect(n, 1), ctx)));
        // catch clauses: flatten into the same block-like list
        const ParseNode *catches = ctx.child(n, 2);
        if (catches) {
            std::vector<StmtPtr> extra;
            auto collectCatches = [&](auto &&self, const ParseNode &node) -> void {
                if (node.children.empty()) return;
                if (node.children.size() == 1) return;     // single catch -> handled by caller pattern below
                if (node.symbol == "catch_clause_list") {
                    if (node.children.size() == 2) { self(self, *node.children[1]); }
                    self(self, *node.children[0]);
                    return;
                }
                if (node.symbol == "catch_clause") {
                    auto st2 = makeStmt(Stmt::BLOCK, node);
                    const ParseNode *param = ctx.child(node, 3);
                    st2->name = (node.children[1]->symbol == "(") ? "" : "";
                    const ParseNode *bodyNode = ctx.child(node, 4);
                    if (bodyNode) st2->body = toStmts(*bodyNode, ctx);
                    extra.push_back(st2);
                }
            };
            collectCatches(collectCatches, *catches);
            st->body.insert(st->body.end(), extra.begin(), extra.end());
        }
        return {st};
    }
    if (s == "for_in_stmt") {
        auto st = makeStmt(Stmt::FOR_IN, n);
        st->type = toType(*ctx.expect(n, 2), ctx);
        st->name = leafText(*ctx.expect(n, 3), ctx);
        st->expr = toExpr  (*ctx.expect(n, 5), ctx);
        st->body.push_back(wrapBlock(toStmts(*ctx.expect(n, 7), ctx)));
        return {st};
    }
    if (s == "for_stmt") {
        auto st = makeStmt(Stmt::FOR, n);
        // for ( for_init expr_opt ; expr_opt ) stmt
        const ParseNode *initNode = ctx.child(n, 2);
        if (initNode && !initNode->children.empty()) {
            const ParseNode *inner = initNode->children[0].get();
            if (inner->symbol == "var_decl") {
                auto decls = toStmts(*inner, ctx);
                if (!decls.empty()) st->init = decls[0];
            } else if (inner->symbol == "expr") {
                st->init = makeStmt(Stmt::EXPR_STMT, n);
                st->init->expr = toExpr(*inner, ctx);
            }
        }
        const ParseNode *condNode = ctx.child(n, 3);
        if (condNode && !condNode->children.empty()) st->cond = toExpr(*condNode->children[0], ctx);
        const ParseNode *incrNode = ctx.child(n, 5);
        if (incrNode && !incrNode->children.empty()) {
            st->incr = makeStmt(Stmt::EXPR_STMT, n);
            st->incr->expr = toExpr(*incrNode->children[0], ctx);
        }
        st->body.push_back(wrapBlock(toStmts(*ctx.expect(n, 7), ctx)));
        return {st};
    }
    if (s == "return_stmt") {
        auto st = makeStmt(Stmt::RETURN, n);
        const ParseNode *eo = ctx.child(n, 1);
        if (eo && !eo->children.empty()) st->expr = toExpr(*eo->children[0], ctx);
        return {st};
    }
    if (s == "break_stmt")    return {makeStmt(Stmt::BREAK, n)};
    if (s == "continue_stmt") return {makeStmt(Stmt::CONTINUE, n)};
    if (s == "empty_stmt")    return {makeStmt(Stmt::EMPTY, n)};
    if (s == "cpp_code") {
        auto st = makeStmt(Stmt::NATIVE_CODE, n);
        const std::string full = leafText(n, ctx);
        st->nativeLang = (full.rfind("<cppCode>", 0) == 0) ? "cpp" : "c";
        st->nativeCode = full;
        return {st};
    }
    return {};
}

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

std::vector<DeclPtr> toDecls(const ParseNode &n, const Ctx &ctx);

std::vector<std::string> collectMods(const ParseNode &n, const Ctx &ctx) {
    std::vector<std::string> out;
    auto walk = [&](auto &&self, const ParseNode &node) -> void {
        if (node.children.empty()) return;
        if (node.children.size() == 1) {
            const std::string &c0 = node.children[0]->symbol;
            if (c0 == "ely_mod" || c0 == "c_mod" || c0 == "cpp_mod" || c0 == "cv" || c0 == "cv_qual") {
                // single modifier (or cv chain)
                if (node.children[0]->children.empty()) {
                    out.push_back(leafText(*node.children[0], ctx));
                } else self(self, *node.children[0]);
            } else {
                self(self, *node.children[0]);
            }
            return;
        }
        if (node.children.size() >= 2) {
            self(self, *node.children[0]);
            self(self, *node.children[1]);
        }
    };
    walk(walk, n);
    return out;
}

std::vector<std::pair<TypeRef, std::string>> toParams(const ParseNode &n, const Ctx &ctx,
                                                      std::vector<ExprPtr> &defaults) {
    std::vector<std::pair<TypeRef, std::string>> out;
    auto walkList = [&](auto &&self, const ParseNode &node) -> void {
        if (node.children.empty()) return;
        if (node.children.size() == 1) { self(self, *node.children[0]); return; }
        if (node.children.size() == 3) {
            self(self, *node.children[0]);
            self(self, *node.children[2]);
            return;
        }
    };
    // param_list -> ε | param | param_list ',' param ; c_param_list similar
    auto addParam = [&](const ParseNode &p) {
        // param -> type id | type id '=' expr | type | type init_declarator
        TypeRef t = toType(*ctx.expect(p, 0), ctx);
        std::string name;
        ExprPtr def;
        const ParseNode *rest = ctx.child(p, 1);
        if (rest) {
            if (rest->symbol == "id") name = leafText(*rest, ctx);
            else if (rest->symbol == "init_declarator") {
                // take id from declarator, propagate ptrs
                const ParseNode *idNode = ctx.child(*rest, 1);
                if (idNode) name = leafText(*idNode, ctx);
                const ParseNode *ptrs = ctx.child(*rest, 0);
                if (ptrs) for (const auto &ch : ptrs->children) {
                    if (ch->symbol == "*") t.pointerDepth++;
                    else if (ch->symbol == "&") t.isReference = true;
                }
                const ParseNode *init = ctx.child(*rest, 3);
                if (init && !init->children.empty()
                    && init->children[0]->symbol == "=") def = toExpr(*init->children[1], ctx);
            }
        }
        if (p.children.size() == 4) {            // type id = expr
            def = toExpr(*ctx.expect(p, 3), ctx);
        }
        out.emplace_back(t, name);
        defaults.push_back(def);
    };

    if (n.children.empty()) return out;
    if (n.children[0]->symbol == "void") return out;
    // walk the comma-list
    auto collect = [&](auto &&self, const ParseNode &node) -> void {
        if (node.symbol == "c_param" || node.symbol == "param") { addParam(node); return; }
        if (node.children.empty()) return;
        if (node.children.size() == 1) { self(self, *node.children[0]); return; }
        if (node.children.size() == 3) {
            self(self, *node.children[0]);
            self(self, *node.children[2]);
            return;
        }
    };
    collect(collect, n);
    return out;
}

std::vector<DeclPtr> toDecls(const ParseNode &n, const Ctx &ctx) {
    std::vector<DeclPtr> out;
    const std::string &s = n.symbol;

    // ---- program lists ------------------------------------------------------
    if (s == "ely_top_list" || s == "c_top_list" || s == "cpp_top_list"
        || s == "ely_class_members" || s == "cpp_members" || s == "c_field_list"
        || s == "var_decl_list") {
        if (n.children.size() >= 2) {
            auto prev = toDecls(*n.children[0], ctx);
            auto tail = toDecls(*n.children[1], ctx);
            out.reserve(prev.size() + tail.size());
            out.insert(out.end(), prev.begin(), prev.end());
            out.insert(out.end(), tail.begin(), tail.end());
        }
        return out;
    }
    if (s == "ely_top" || s == "c_top" || s == "cpp_top"
        || s == "ely_class_member" || s == "cpp_member") {
        return toDecls(*ctx.expect(n, 0), ctx);
    }
    if (s == "access_spec") return {};   // public:/private:/protected: — no node for now

    // ---- using --------------------------------------------------------------
    if (s == "using_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::USING;
        const ParseNode *t = ctx.child(n, 1);
        if (t) d->modulePath = (t->children.empty()) ? leafText(*t, ctx) : idPathText(*t, ctx);
        out.push_back(d);
        return out;
    }
    if (s == "cpp_using_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::USING;
        if (n.children.size() >= 3 && n.children[1]->symbol == "namespace") {
            d->modulePath = "namespace " + idPathText(*n.children[2], ctx);
        } else if (n.children.size() >= 2 && n.children[0]->symbol == "using") {
            d->modulePath = idPathText(*n.children[1], ctx);
        } else if (n.children.size() >= 4) {
            d->kind = Decl::ALIAS;
            d->name = idPathText(*n.children[1], ctx);
            d->aliasType = toType(*n.children[3], ctx);
        }
        out.push_back(d);
        return out;
    }

    // ---- variables ----------------------------------------------------------
    if (s == "var_decl" || s == "c_field") {
        for (auto &dl : parseVarDecl(n, ctx)) {
            auto d = std::make_shared<Decl>();
            d->kind = Decl::VARIABLE;
            d->name = dl.name;
            d->ret = dl.type;
            d->pos = ::Elica::Core::Position();
            if (dl.hasArr) d->ret.words.push_back("[]");
            d->modifiers.push_back("var");
            out.push_back(d);
        }
        return out;
    }
    // P0.1: Ely field declaration `[mods] var_decl` (wait/unwait/...)
    if (s == "ely_var_decl") {
        const ParseNode *mods = ctx.child(n, 0);
        const ParseNode *vd = ctx.child(n, 1);
        std::vector<DeclPtr> result;
        if (vd) result = toDecls(*vd, ctx);
        if (mods) {
            auto modList = collectMods(*mods, ctx);
            for (auto &d : result) {
                d->modifiers.insert(d->modifiers.end(), modList.begin(), modList.end());
            }
        }
        return result;
    }
    if (s == "ely_var_decl_list") {
        if (n.children.size() >= 2) {
            auto prev = toDecls(*n.children[0], ctx);
            auto tail = toDecls(*n.children[1], ctx);
            out.reserve(prev.size() + tail.size());
            out.insert(out.end(), prev.begin(), prev.end());
            out.insert(out.end(), tail.begin(), tail.end());
        }
        return out;
    }

    // ---- functions ----------------------------------------------------------
    if (s == "ely_func_decl" || s == "c_func_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::FUNCTION;
        const bool ely = (s == "ely_func_decl");
        d->isElyFuncStyle = ely;
        d->modifiers = collectMods(*ctx.expect(n, 0), ctx);
        d->ret = toType(*ctx.expect(n, 1), ctx);
        
        size_t nameIdx = ely ? 3 : 2;
        size_t paramsIdx = ely ? 6 : 4;  // сдвинуто из-за ely_template_params
        size_t bodyIdx = ely ? 8 : 6;
        
        const ParseNode *nameNode = ctx.expect(n, nameIdx);
        d->name = leafText(*nameNode, ctx);
        
        // Extract template params for Ely
        if (ely) {
            const ParseNode *tpl = ctx.child(n, 4);  // ely_template_params
            if (tpl && !tpl->children.empty()) {
                const ParseNode &list = *tpl->children[1];  // template_param_list
                auto collect = [&](auto &&self, const ParseNode &node) -> void {
                    if (node.children.empty()) return;
                    if (node.children.size() == 1) { self(self, *node.children[0]); return; }
                    if (node.children.size() == 3) { self(self, *node.children[0]); self(self, *node.children[2]); return; }
                    const ParseNode *idNode = ctx.child(node, 0);
                    if (idNode && idNode->symbol == "id") {
                        d->templateParams.push_back(leafText(*idNode, ctx));
                    }
                };
                collect(collect, list);
            }
        }
        
        const ParseNode *pl = ctx.child(n, paramsIdx);
        if (pl) {
            std::vector<ExprPtr> defaults;
            d->params = toParams(*pl, ctx, defaults);
            d->defaultArgs = std::move(defaults);
        }
        const ParseNode *bodyNode = ctx.child(n, bodyIdx);
        if (bodyNode && bodyNode->symbol == "block") d->body = wrapBlock(toStmts(*bodyNode, ctx));
        out.push_back(d);
        return out;
    }
    if (s == "cpp_func_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::FUNCTION;
        d->modifiers = collectMods(*ctx.expect(n, 0), ctx);
        const std::string &second = ctx.expect(n, 1)->symbol;
        const ParseNode *pl = nullptr;

        if (second == "~") {                        // destructor
            d->isDtor = true;
            d->name = leafText(*ctx.expect(n, 2), ctx);
        } else if (second == "operator") {          // operator overload (no return type)
            d->isOperator = true;
            d->operatorString = opText(*ctx.expect(n, 2));   // op_sym
            pl = ctx.child(n, 4);
        } else if (second == "type") {              // regular function or operator overload
            const ParseNode *third = ctx.child(n, 2);
            if (third && third->symbol == "operator") {
                // `ret operator op_sym ( params ) cv block`
                d->isOperator = true;
                d->ret = toType(*ctx.expect(n, 1), ctx);
                d->operatorString = opText(*ctx.expect(n, 3));   // op_sym
                pl = ctx.child(n, 5);
            } else {
                d->ret = toType(*ctx.expect(n, 1), ctx);
                d->name = idPathText(*ctx.expect(n, 2), ctx);
                pl = ctx.child(n, 4);
            }
        } else {                                    // constructor
            d->isCtor = true;
            d->name = leafText(*ctx.expect(n, 1), ctx);
            pl = ctx.child(n, 3);
        }
        if (pl) {
            std::vector<ExprPtr> defaults;
            d->params = toParams(*pl, ctx, defaults);
            d->defaultArgs = std::move(defaults);
        }
        // body: direct `block` child (ely/c) or wrapped in `block_or_semi` (cpp)
        for (const auto &ch : n.children) {
            if (ch->symbol == "block") { d->body = wrapBlock(toStmts(*ch, ctx)); break; }
            if (ch->symbol == "block_or_semi" && !ch->children.empty()
                && ch->children[0]->symbol == "block") {
                d->body = wrapBlock(toStmts(*ch->children[0], ctx));
                break;
            }
        }
        out.push_back(d);
        return out;
    }

    // ---- classes / structs --------------------------------------------------
    if (s == "ely_class_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::CLASS;
        d->modifiers = collectMods(*ctx.expect(n, 0), ctx);
        d->name = leafText(*ctx.expect(n, 2), ctx);
        
        // Extract template params
        const ParseNode *tpl = ctx.child(n, 3);  // ely_template_params
        if (tpl && !tpl->children.empty()) {
            const ParseNode &list = *tpl->children[1];
            auto collect = [&](auto &&self, const ParseNode &node) -> void {
                if (node.children.empty()) return;
                if (node.children.size() == 1) { self(self, *node.children[0]); return; }
                if (node.children.size() == 3) { self(self, *node.children[0]); self(self, *node.children[2]); return; }
                const ParseNode *idNode = ctx.child(node, 0);
                if (idNode && idNode->symbol == "id") {
                    d->templateParams.push_back(leafText(*idNode, ctx));
                }
            };
            collect(collect, list);
        }
        
        // ely_class_bases at index 4 (сдвинуто)
        const ParseNode *bases = ctx.child(n, 4);
        if (bases && !bases->children.empty()) {
            const ParseNode *bl = ctx.expect(*bases, 1);
            auto collectBases = [&](auto &&self, const ParseNode &node) -> void {
                if (node.symbol == "ely_base_list") {
                    if (node.children.size() == 3) { self(self, *node.children[0]); self(self, *node.children[2]); return; }
                    self(self, *node.children[0]);
                    return;
                }
                if (node.symbol == "ely_base") {
                    const ParseNode *name = node.children.back().get();
                    std::string b = (name->symbol == "id_path") ? idPathText(*name, ctx) : leafText(*name, ctx);
                    if (!b.empty()) d->bases.push_back(b);
                }
            };
            collectBases(collectBases, *bl);
            if (!d->bases.empty()) d->baseName = d->bases.front();
        }
        
        const ParseNode *members = ctx.child(n, 6);  // сдвинуто из-за template params
        if (members) d->members = toDecls(*members, ctx);
        out.push_back(d);
        return out;
    }
    if (s == "struct_decl") {                       // Ely struct { var_decls }
        auto d = std::make_shared<Decl>();
        d->kind = Decl::STRUCT;
        d->isStruct = true;
        d->modifiers = collectMods(*ctx.expect(n, 0), ctx);
        d->name = leafText(*ctx.expect(n, 2), ctx);
        const ParseNode *members = ctx.child(n, 4);
        if (members) d->members = toDecls(*members, ctx);
        out.push_back(d);
        return out;
    }
    if (s == "cpp_class_decl" || s == "cpp_struct_decl" || s == "c_struct_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::CLASS;
        d->isStruct = (s != "cpp_class_decl");
        if (s == "cpp_class_decl" || s == "cpp_struct_decl") {
            const ParseNode &head = *ctx.expect(n, 0);      // cpp_class_head
            d->modifiers = collectMods(*ctx.expect(head, 0), ctx);
            const ParseNode *kw = ctx.child(head, 1);
            d->isStruct = (kw && kw->symbol == "struct");
            d->name = leafText(*ctx.expect(head, 2), ctx);
            const ParseNode *bases = ctx.child(head, 3);
            if (bases && !bases->children.empty()) {
                // collect ALL direct bases (base_list -> base_list ',' base | base)
                const ParseNode *bl = ctx.expect(*bases, 1);
                auto collectBases = [&](auto &&self, const ParseNode &node) -> void {
                    if (node.children.empty()) return;
                    if (node.symbol == "base_list") {
                        if (node.children.size() == 3) {       // base_list , base
                            self(self, *node.children[0]);
                            self(self, *node.children[2]);
                            return;
                        }
                        self(self, *node.children[0]);
                        return;
                    }
                    if (node.symbol == "base") {
                        // ['public'|'protected'|'private'|'virtual'] id_path
                        const ParseNode *name = node.children.back().get();
                        std::string b = (name->symbol == "id_path")
                            ? idPathText(*name, ctx)
                            : leafText(*name, ctx);
                        if (!b.empty()) d->bases.push_back(b);
                    }
                };
                collectBases(collectBases, *bl);
                if (!d->bases.empty()) d->baseName = d->bases.front();   // legacy
            }
            const ParseNode *members = ctx.child(n, 2);
            if (members) d->members = toDecls(*members, ctx);
        } else { // c_struct_decl
            d->name = leafText(*ctx.expect(n, 1), ctx);
            const ParseNode *members = ctx.child(n, 3);
            if (members) d->members = toDecls(*members, ctx);
        }
        out.push_back(d);
        return out;
    }

    // ---- function and classes generic params -------------------------------

    if (s == "ely_template_params") {
        std::vector<std::string> params;    
        if (n.children.size() == 3) {  // < template_param_list >
            const ParseNode &list = *n.children[1];
            auto collect = [&](auto &&self, const ParseNode &node) -> void {
                if (node.children.empty()) return;
                if (node.children.size() == 1) {
                    self(self, *node.children[0]);
                    return;
                }
                if (node.children.size() == 3) {  // list , param
                    self(self, *node.children[0]);
                    self(self, *node.children[2]);
                    return;
                }
                // template_param leaf
                const ParseNode *idNode = ctx.child(node, 0);
                if (idNode && idNode->symbol == "id") {
                    params.push_back(leafText(*idNode, ctx));
                }
            };
            collect(collect, list);
        }
        // Store params in a thread-local or return via out param
        // For now, we'll extract them directly in ely_func_decl/ely_class_decl
        return {};  // placeholder — actual extraction happens inline
    }

    // ---- namespace / enum / typedef / template / extern ---------------------
    if (s == "ely_namespace_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::NAMESPACE;
        if (n.children.size() >= 4 && n.children[1]->symbol == "id_path") {
            // namespace id_path { ely_top_list }
            d->name = idPathText(*n.children[1], ctx);
            const ParseNode *body = ctx.child(n, 3);
            if (body) d->nested = toDecls(*body, ctx);
        } else {
            // namespace id = id_path ;
            d->name = leafText(*ctx.expect(n, 1), ctx);
            d->modulePath = leafText(*ctx.expect(n, 3), ctx);
        }
        out.push_back(d);
        return out;
    }
    if (s == "ely_template_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::TEMPLATE;
        const ParseNode *params = ctx.child(n, 2);          // ely_template_params
        if (params) {
            auto collect = [&](auto &&self, const ParseNode &node) -> void {
                if (node.children.empty()) return;
                if (node.symbol == "ely_template_params") {
                    if (node.children.size() == 3) {
                        self(self, *node.children[0]);
                        self(self, *node.children[2]);
                        return;
                    }
                    self(self, *node.children[0]);
                    return;
                }
                if (node.symbol == "ely_template_param") {
                    const ParseNode *last = node.children.back().get();
                    if (last && last->symbol == "id" && last->children.empty())
                        d->templateParams.push_back(leafText(*last, ctx));
                    else if (last)
                        d->templateParams.push_back(leafText(*last, ctx));
                }
            };
            collect(collect, *params);
        }
        const ParseNode *inner = ctx.child(n, 4);
        if (inner) d->nested = toDecls(*inner, ctx);
        out.push_back(d);
        return out;
    }
    if (s == "namespace_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::NAMESPACE;
        d->name = leafText(*ctx.expect(n, 1), ctx);
        const ParseNode *body = ctx.child(n, 3);
        if (body) d->nested = toDecls(*body, ctx);
        out.push_back(d);
        return out;
    }
    if (s == "c_enum_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::ENUM;
        d->name = leafText(*ctx.expect(n, 1), ctx);
        out.push_back(d);
        return out;
    }
    if (s == "c_typedef_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::TYPEDEF;
        d->aliasType = toType(*ctx.expect(n, 1), ctx);
        const ParseNode *decl = ctx.expect(n, 2);      // init_declarator
        // init_declarator: ptrs id ... | ptrs ( * id ) ( params ) cv | ...
        const ParseNode *idNode = nullptr;
        if (decl && !decl->children.empty()) {
            // search the declarator tree for the id that is the typedef name
            std::function<const ParseNode*(const ParseNode&)> findName =
                [&](const ParseNode &node) -> const ParseNode* {
                if (node.children.empty()) {
                    return node.symbol == "id" ? &node : nullptr;
                }
                if (node.symbol == "id") return &node;
                for (const auto &ch : node.children) {
                    if (const ParseNode *r = findName(*ch)) return r;
                }
                return nullptr;
            };
            idNode = findName(*decl);
        }
        d->name = idNode ? leafText(*idNode, ctx) : "?";
        out.push_back(d);
        return out;
    }
    if (s == "template_decl") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::TEMPLATE;
        const ParseNode *params = ctx.child(n, 2);
        if (params) {
            auto collect = [&](auto &&self, const ParseNode &node) -> void {
                if (node.children.empty()) return;
                if (node.children.size() == 1) { self(self, *node.children[0]); return; }
                if (node.children.size() == 3) { self(self, *node.children[0]); self(self, *node.children[2]); return; }
                // template_param leaf
                const ParseNode *last = node.children[node.children.size() - 1].get();
                if (last && last->symbol == "id" && last->children.empty())
                    d->templateParams.push_back(leafText(*last, ctx));
            };
            collect(collect, *params);
        }
        const ParseNode *inner = ctx.child(n, 4);
        if (inner) d->nested = toDecls(*inner, ctx);
        out.push_back(d);
        return out;
    }
    if (s == "extern_block") {
        auto d = std::make_shared<Decl>();
        d->kind = Decl::EXTERN_BLOCK;
        const ParseNode *body = ctx.child(n, 3);
        if (body) d->nested = toDecls(*body, ctx);
        out.push_back(d);
        return out;
    }

    if (s == ";") return {};
    return {};
}

} // namespace

// ===========================================================================
// Token pre-processing
// ===========================================================================

std::vector<Lex::Token> collapseNativeBlocks(
    const std::vector<Lex::Token> &tokens,
    std::vector<std::string> &langs) {
    std::vector<Lex::Token> out;
    langs.clear();
    size_t i = 0;
    const size_t n = tokens.size();
    while (i < n) {
        const auto &t = tokens[i];
        bool isBlockKw = false;
        std::string lang;
        if (std::holds_alternative<Lex::ElyToken>(t.type)) {
            const auto et = std::get<Lex::ElyToken>(t.type);
            if (et == Lex::ElyToken::KW_cppCode) { isBlockKw = true; lang = "cpp"; }
            else if (et == Lex::ElyToken::KW_cCode) { isBlockKw = true; lang = "c"; }
        }
        if (!isBlockKw) { out.push_back(t); ++i; continue; }

        // find matching '}' (lexer emits { ... } around the block)
        size_t j = i + 1;
        int brace = 0;
        while (j < n) {
            if (std::holds_alternative<Lex::CommonToken>(tokens[j].type)) {
                const auto ct = std::get<Lex::CommonToken>(tokens[j].type);
                if (ct == Lex::CommonToken::LBRACE) brace++;
                else if (ct == Lex::CommonToken::RBRACE) {
                    if (brace == 0) break;      // unbalanced — stop here
                    brace--;
                    if (brace == 0) break;     // this RBRACE closes the block
                }
            }
            ++j;
        }
        // j is at the closing RBRACE (never consumed)
        std::string raw;
        for (size_t k = i + 1; k < j; ++k) {
            if (k > i + 1) raw += ' ';
            raw += tokens[k].lexeme;
        }
        Lex::Token collapsed(Lex::TokenVariant(Lex::CommonToken::NATIVE_CODE),
                             "<" + lang + "Code>" + raw, t.position);
        out.push_back(collapsed);
        langs.push_back(lang);
        i = (j < n) ? j + 1 : n;   // skip past '}'
    }
    return out;
}

std::vector<Lex::Token> extractDirectives(
    const std::vector<Lex::Token> &tokens,
    std::vector<std::string> &directives) {
    std::vector<Lex::Token> out;
    directives.clear();
    size_t i = 0;
    const size_t n = tokens.size();
    while (i < n) {
        const auto &t = tokens[i];
        const bool isHash = std::holds_alternative<Lex::CommonToken>(t.type)
            && std::get<Lex::CommonToken>(t.type) == Lex::CommonToken::HASH
            && t.position.column == 1;
        if (!isHash) { out.push_back(t); ++i; continue; }

        // directive line: consume until the line number changes
        std::string line;
        const size_t startLine = t.position.line;
        while (i < n && tokens[i].position.line == startLine) {
            if (!line.empty()) line += ' ';
            line += tokens[i].lexeme;
            ++i;
        }
        directives.push_back(line);

        // lose the consumed directive tokens — hmm they are not in `out`
    }
    return out;
}

// ===========================================================================
// Parse driver / program building
// ===========================================================================

namespace {
// P1.5: parse an interpolation `{expr}` fragment (f-strings). We lex the raw
// text with the Ely lexer and parse it against the shared `expr` rule.
ExprPtr parseElyInterpolated(const std::string &frag, Lex::LexerLanguage lang) {
    auto simple = Expr::make(Expr::RAW, frag, ::Elica::Core::Position());
    if (frag.empty()) return simple;

    Lex::Lexer lx(frag, lang);
    auto toks = lx.tokenize();
    std::vector<Lex::Token> input;
    input.reserve(toks.size());
    for (const auto &t : toks) {
        bool isEof = std::holds_alternative<Lex::CommonToken>(t.type)
            && std::get<Lex::CommonToken>(t.type) == Lex::CommonToken::END_OF_FILE;
        if (isEof) break;
        input.push_back(t);
    }
    if (input.empty()) return simple;

    // Build the right grammar and re-target the start symbol onto `expr`.
    Earley::Grammar g;
    switch (lang) {
        case Lex::LexerLanguage::ELY: g = buildElyGrammar(); break;
        case Lex::LexerLanguage::C:   g = buildCGrammar();  break;
        default:                      g = buildCppGrammar(); break;
    }
    g.start = "expr";

    Earley::EarleyParser p;
    auto tree = p.parse(input, g);
    if (!tree) return simple;
    const auto &root = *tree;
    if (!root || root->children.empty()) return simple;
    Ctx cctx{input};
    return toExpr(*root->children[0], cctx);
}

// split "hello {name}!" into ["hello ", Name, "!"] — recursive, brace-bounded.
std::vector<ExprPtr> interpParts(const std::string &raw, Lex::LexerLanguage lang) {
    std::vector<ExprPtr> parts;
    std::string lit;
    size_t i = 0;
    auto flush = [&] {
        if (!lit.empty()) {
            auto s = Expr::make(Expr::STR_LIT, lit, ::Elica::Core::Position());
            parts.push_back(s);
            lit.clear();
        }
    };
    while (i < raw.size()) {
        if (raw[i] == '{') {
            // find matching '}' (brace-bounded; nested braces supported)
            int depth = 0; size_t j = i;
            for (; j < raw.size(); ++j) {
                if (raw[j] == '{') depth++;
                else if (raw[j] == '}') { depth--; if (depth == 0) break; }
            }
            if (depth == 0 && j < raw.size()) {
                flush();
                parts.push_back(parseElyInterpolated(raw.substr(i+1, j-i-1), lang));
                i = j + 1;
                continue;
            }
        }
        lit.push_back(raw[i++]);
    }
    flush();
    return parts;
}
} // namespace

std::shared_ptr<Program> buildProgram(
    const Earley::ParseNode &tree,
    const std::vector<Lex::Token> &tokens,
    const std::string &fileName) {
    Ctx ctx{tokens};
    auto prog = std::make_shared<Program>();
    prog->fileName = fileName;
    if (tree.children.empty()) return prog;
    const ParseNode &list = *tree.children[0];   // *_top_list
    // top list reachable via children[0] (single) - handle recursion
    std::vector<const ParseNode*> topNodes;
    auto collect = [&](auto &&self, const ParseNode &node) -> void {
        if (node.children.size() >= 2) {
            self(self, *node.children[0]);
            topNodes.push_back(node.children[1].get());
        } else if (node.children.size() == 1) {
            topNodes.push_back(node.children[0].get());
        }
    };
    collect(collect, list);
    for (const ParseNode *top : topNodes) {
        auto decls = toDecls(*top, ctx);
        for (auto &d : decls) prog->decls.push_back(d);
    }
    return prog;
}

// ===========================================================================
// Human-readable AST dump
// ===========================================================================

namespace {

std::string exprStr(const Expr &e);

std::string typeToString(const TypeRef &t) { return t.toString(); }

std::string joinArgs(const std::vector<ExprPtr> &args) {
    std::string s;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) s += ", ";
        s += exprStr(*args[i]);
    }
    return s;
}

std::string exprStr(const Expr &e) {
    // Nodes built via Expr::make() store their operator in `text`; be robust
    // and prefer `op`, falling back to `text`.
    const std::string &op = e.op.empty() ? e.text : e.op;
    switch (e.kind) {
        case Expr::INT_LIT: case Expr::FLT_LIT: case Expr::BOOL_LIT:
        case Expr::IDENT: return e.text;
        case Expr::CHAR_LIT: return e.text == "\0" ? "'\\0'" : "'" + e.text + "'";
        case Expr::INTERP_STR: {
            std::string s = "f\"";
            for (const auto &p : e.parts) {
                if (p->kind == Expr::STR_LIT) s += p->text;
                else s += "{" + exprStr(*p) + "}";
            }
            return s + "\"";
        }
        case Expr::STR_LIT: return "\"" + e.text + "\"";
        case Expr::THIS: return "this";
        case Expr::NULLPTR: return "nullptr";
        case Expr::BINARY: return "(" + exprStr(*e.lhs) + " " + op + " " + exprStr(*e.rhs) + ")";
        case Expr::UNARY:
            if (e.newType.words.empty() && op == "sizeof") return "sizeof(" + exprStr(*e.lhs) + ")";
            if (op == "delete") return e.newBrackets ? "delete[] " + exprStr(*e.lhs)
                                                     : "delete " + exprStr(*e.lhs);
            if (op == "sizeof") return "sizeof(" + typeToString(e.newType) + ")";
            return op + exprStr(*e.lhs);
        case Expr::ASSIGN:
            if (op == "named") return exprStr(*e.target) + "=" + exprStr(*e.value);
            return exprStr(*e.target) + " " + op + " " + exprStr(*e.value);
        case Expr::TERNARY: return "(" + exprStr(*e.cond) + " ? " + exprStr(*e.thenBranch) + " : " + exprStr(*e.elseBranch) + ")";
        case Expr::CALL: return exprStr(*e.callee) + "(" + joinArgs(e.args) + ")";
        case Expr::INDEX: return exprStr(*e.base) + "[" + exprStr(*e.index) + "]";
        case Expr::MEMBER: {
            std::string s = exprStr(*e.base);
            const char *sep = e.isSafeNav ? "?."
                            : e.isArrow   ? "->"
                            : (op == "::") ? "::" : ".";
            for (const auto &m : e.members) s += std::string(sep) + m;
            return s;
        }
        case Expr::INC_DEC: return e.isPrefix ? op + exprStr(*e.lhs) : exprStr(*e.lhs) + op;
        case Expr::NEW_EXPR: {
            if (!e.newType.empty()) {
                std::string s = "new " + typeToString(e.newType);
                if (e.newBrackets) s += e.arraySize ? "[" + exprStr(*e.arraySize) + "]" : "[]";
                return s;
            }
            return "new " + exprStr(*e.target) + "(" + joinArgs(e.args) + ")";
        }
        case Expr::SIZEOF:
            if (!e.newType.empty()) return "sizeof(" + typeToString(e.newType) + ")";
            return "sizeof(" + exprStr(*e.lhs) + ")";
        case Expr::NATIVE: return "<native>";
    }
    return "?";
}

void printExpr(std::ostream &os, const Expr &e) { os << exprStr(e); }

void printStmt(std::ostream &os, const Stmt &s, int depth) {
    const std::string pad(static_cast<size_t>(depth) * 2, ' ');
    switch (s.kind) {
        case Stmt::BLOCK:
            os << pad << "{\n";
            for (const auto &st : s.body) printStmt(os, *st, depth + 1);
            os << pad << "}\n";
            return;
        case Stmt::EXPR_STMT:
            os << pad; printExpr(os, *s.expr); os << ";\n"; return;
        case Stmt::VAR_DECL:
            os << pad << typeToString(s.type) << " " << s.name;
            if (s.initExpr) {
                // Foo a(1); — constructor-call initializer, not assignment
                const Expr &init = *s.initExpr;
                const bool isCallInit = init.kind == Expr::CALL && init.callee
                    && init.callee->kind == Expr::IDENT && init.callee->text == s.name;
                if (isCallInit) {
                    os << "(" << joinArgs(init.args) << ")";
                } else {
                    os << " = ";
                    printExpr(os, init);
                }
            }
            os << ";\n"; return;
        case Stmt::IF: {
            os << pad << "if (";
            if (s.cond) printExpr(os, *s.cond);
            os << ") ";
            printStmt(os, *s.thenBranch, depth);
            if (s.elseBranch) {
                os << pad << "else ";
                printStmt(os, *s.elseBranch, depth);
            }
            return;
        }
        case Stmt::WHILE:
            os << pad << "while (";
            if (s.cond) printExpr(os, *s.cond);
            os << ") ";
            printStmt(os, *s.body[0], depth);
            return;
        case Stmt::FOR_IN:
            os << pad << "for (" << typeToString(s.type) << " " << s.name << " in ";
            if (s.expr) printExpr(os, *s.expr);
            os << ") ";
            printStmt(os, *s.body[0], depth);
            return;
        case Stmt::FOR:
            os << pad << "for (";
            if (s.init) {
                if (s.init->kind == Stmt::VAR_DECL) {
                    os << typeToString(s.init->type) << " " << s.init->name;
                    if (s.init->initExpr) { os << " = "; printExpr(os, *s.init->initExpr); }
                } else if (s.init->expr) {
                    printExpr(os, *s.init->expr);
                }
                os << "; ";
            } else {
                os << "; ";
            }
            if (s.cond) { printExpr(os, *s.cond); os << "; "; } else os << "; ";
            if (s.incr && s.incr->expr) printExpr(os, *s.incr->expr);
            os << ") ";
            printStmt(os, *s.body[0], depth);
            return;
        case Stmt::RETURN:
            os << pad << "return";
            if (s.expr) { os << " "; printExpr(os, *s.expr); }
            os << ";\n"; return;
        case Stmt::BREAK: os << pad << "break;\n"; return;
        case Stmt::CONTINUE: os << pad << "continue;\n"; return;
        case Stmt::EMPTY: os << pad << ";\n"; return;
        case Stmt::DO:
            os << pad << "do ";
            printStmt(os, *s.body[0], depth);
            os << pad << "while (";
            if (s.cond) printExpr(os, *s.cond);
            os << ");\n";
            return;
        case Stmt::SWITCH:
            os << pad << "switch (";
            if (s.cond) printExpr(os, *s.cond);
            os << ") ";
            if (!s.body.empty()) printStmt(os, *s.body[0], depth);
            else os << "{ }\n";
            return;
        case Stmt::GOTO:
            os << pad << "goto " << s.label << ";\n"; return;
        case Stmt::LABEL:
            os << pad << s.label << ":\n";
            for (const auto &st : s.body) printStmt(os, *st, depth + 1);
            return;
        case Stmt::RAW:
            os << pad << s.raw << "\n"; return;
        case Stmt::NATIVE_CODE:
            os << pad << "<" << s.nativeLang << "Code> " << s.nativeCode << "\n";
            return;
    }
}

void printParams(std::ostream &os, const Decl &d) {
    os << "(";
    for (size_t i = 0; i < d.params.size(); ++i) {
        if (i) os << ", ";
        os << d.params[i].first.toString() << " " << d.params[i].second;
    }
    os << ")";
}

void printMods(std::ostream &os, const Decl &d) {
    for (const auto &m : d.modifiers) if (!m.empty()) os << m << " ";
}

void printDecl(std::ostream &os, const Decl &d, int depth) {
    const std::string pad(static_cast<size_t>(depth) * 2, ' ');
    switch (d.kind) {
        case Decl::FUNCTION: {
            os << pad << "func ";
            printMods(os, d);
            if (!d.ret.empty()) { os << d.ret.toString() << " "; }
            if (d.isCtor) { os << d.name << "(ctor)"; }
            else if (d.isDtor) { os << d.name << "(dtor)"; }
            else if (d.isOperator) { os << "operator" << d.operatorString; }
            else os << d.name;
            printParams(os, d);
            os << "\n";
            if (d.body) printStmt(os, *d.body, depth + 1);
            else os << pad << "  ; (declaration only)\n";
            return;
        }
        case Decl::VARIABLE:
            os << pad << "var " << d.ret.toString() << " " << d.name;
            if (!d.modifiers.empty() && d.modifiers.back() == "var") os << "";
            os << "\n";
            return;
        case Decl::CLASS:
            os << pad << (d.isStruct ? "struct " : "class ") << d.name;
            if (!d.baseName.empty()) os << " : " << d.baseName;
            os << " {\n";
            for (const auto &m : d.members) printDecl(os, *m, depth + 1);
            os << pad << "}\n";
            return;
        case Decl::NAMESPACE:
            os << pad << "namespace " << d.name << " {\n";
            for (const auto &m : d.nested) printDecl(os, *m, depth + 1);
            os << pad << "}\n";
            return;
        case Decl::USING:
            os << pad << "using " << d.modulePath << ";\n"; return;
        case Decl::TYPEDEF:
            os << pad << "typedef " << d.aliasType.toString() << " " << d.name << ";\n"; return;
        case Decl::ENUM:
            os << pad << "enum " << d.name << ";\n"; return;
        case Decl::TEMPLATE:
            os << pad << "template <";
            for (size_t i = 0; i < d.templateParams.size(); ++i) {
                if (i) os << ", ";
                os << d.templateParams[i];
            }
            os << ">\n";
            for (const auto &m : d.nested) printDecl(os, *m, depth + 1);
            return;
        case Decl::EXTERN_BLOCK:
            os << pad << "extern \"C\" {\n";
            for (const auto &m : d.nested) printDecl(os, *m, depth + 1);
            os << pad << "}\n";
            return;
        case Decl::ALIAS:
            os << pad << "using " << d.name << " = " << d.aliasType.toString() << ";\n"; return;
    }
}

} // namespace

void printProgram(const Program &prog, std::ostream &os) {
    os << "Program '" << prog.fileName << "'\n";
    for (const auto &d : prog.decls) printDecl(os, *d, 1);
}

} // namespace Elica::Parsing