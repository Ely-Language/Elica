// SPDX-License-Identifier: MIT
#include "semantic/analyzer.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace Elica::Semantic {

namespace {

using Elica::Parsing::Decl;
using Elica::Parsing::Expr;
using Elica::Parsing::ExprPtr;
using Elica::Parsing::Program;
using Elica::Parsing::Stmt;
using Elica::Parsing::StmtPtr;
using Elica::Parsing::TypeRef;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::string pos(const std::string &file, size_t line) {
    return file + (line ? (":" + std::to_string(line)) : "");
}

bool isNumType(const std::string &t) {
    return t == "int" || t == "flt" || t == "double" || t == "float"
        || t == "byte" || t == "ubyte" || t == "short" || t == "ushort"
        || t == "uint" || t == "more" || t == "umore" || t == "long";
}

bool isBuiltinType(const std::string &t) {
    return t == "void" || t == "bool" || t == "str" || t == "char" || t == "any"
        || t == "atom" || t == "sentinel" || isNumType(t);
}

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

struct ClassInfo {
    std::string name;
    std::string file;
    size_t line = 0;
    std::vector<SymbolInfo> funcs;    // methods (incl. ctors)
    std::vector<SymbolInfo> vars;     // fields
    std::map<std::string, const SymbolInfo*> byName;
    std::vector<std::string> directBases;
    std::vector<std::string> ancestors;   // flattened chain, parent first
    bool ancestryBuilt = false;

    void rebuild() {
        byName.clear();
        for (const auto &f : funcs) byName[f.name] = &f;
        for (const auto &v : vars)  byName[v.name] = &v;
    }
};

struct Analyzer {
    AnalysisResult out;

    std::vector<SymbolInfo> globals;             // functions & global vars & typedefs
    std::map<std::string, const SymbolInfo*> gByFileLine; // position dedupe
    std::map<std::string, ClassInfo> classes;    // by class name
    std::map<std::string, std::string> typedefs; // alias -> base
    std::set<std::string> builtins;              // all known type tokens
    bool hasNative = false;                      // project contains C/C++ code

    Analyzer() {
        for (const char *t : {"int", "flt", "str", "void", "bool", "char", "double",
                              "float", "byte", "ubyte", "short", "ushort", "uint",
                              "more", "umore", "any", "atom", "sentinel", "auto"}) {
            builtins.insert(t);
        }
    }

    void error(const std::string &file, size_t line, const std::string &msg) {
        out.diagnostics.push_back({Diagnostic::ERROR, file, line, msg});
        out.errors++;
    }
    void warn(const std::string &file, size_t line, const std::string &msg) {
        out.diagnostics.push_back({Diagnostic::WARN, file, line, msg});
        out.warnings++;
    }

    // ---- type resolution --------------------------------------------------

    std::string canonicalType(const TypeRef &t) const {
        return t.toString();
    }

    // Normalize a qualified name so `mod.Class` and `mod::Class` and
    // `mod Sub` (multi-word) all find the same registered class.
    static std::string normName(std::string name) {
        for (char &c : name) if (c == '.' || c == ' ' || c == '\t') c = ':';
        // collapse ":::" from replacing "." and " " adjacent separators
        std::string out;
        bool prevColon = false;
        for (char c : name) {
            if (c == ':') {
                if (!prevColon) out += ':';
                prevColon = true;
            } else {
                out += c;
                prevColon = false;
            }
        }
        return out;
    }

    // Resolve a type token to a known type: typedef -> base, class -> itself.
    std::string resolveType(const std::string &tok) const {
        auto it = typedefs.find(normName(tok));
        if (it != typedefs.end()) return it->second;
        if (isBuiltinType(tok)) return tok;
        if (classes.count(normName(tok))) return normName(tok);
        
        // C++ qualified type (e.g. std::string, std::vector<int>)
        // If project has native code, accept unknown qualified types
        if (hasNative && (tok.find("::") != std::string::npos || tok.find(".") != std::string::npos)) {
            return tok;  // accept as-is, codegen will handle
        }
        
        // unknown — still valid in C++ space
        return tok;
    }

    // ---- declaration collection -------------------------------------------

    void declareVar(SymbolInfo v, bool isGlobal) {
        (void)isGlobal;
        out.symbols.push_back(v);
    }

    void collectDecl(const std::shared_ptr<Decl> &d, const std::string &file,
                     const std::string &qual = "", bool native = false) {
        if (!d) return;
        if (native) hasNative = true;
        const std::string qname =
            qual.empty() ? d->name : (qual + "::" + d->name);
        switch (d->kind) {
            case Decl::TYPEDEF: {
                std::string base = canonicalType(d->aliasType);
                auto it = typedefs.find(normName(qname));
                if (it != typedefs.end() && it->second != base) {
                    error(file, d->pos.line, "redefinition of typedef '" + d->name + "'");
                } else if (it == typedefs.end()) {
                    typedefs[normName(qname)] = base;
                    builtins.insert(normName(qname));
                }
                SymbolInfo s{d->name, base, "typedef", file, d->pos.line, false, true, {}, "", true, native};
                out.symbols.push_back(s);
                return;
            }
            case Decl::TEMPLATE: {
                std::string tname = qname.empty() ? d->name : qname;
                builtins.insert(tname);
                for (const auto &inner : d->nested) {
                    collectDecl(inner, file, qual, native);
                }
                return;
            }
            case Decl::CLASS:
            case Decl::STRUCT: {
                const std::string key = normName(qname);
                if (classes.count(key)) {
                    error(file, d->pos.line, "duplicate class/struct '" + d->name + "'");
                    return;
                }
                ClassInfo ci;
                ci.name = key;
                ci.file = file;
                ci.line = d->pos.line;
                for (const auto &b : d->bases) {
                    ci.directBases.push_back(normName(b));
                }
                for (const auto &m : d->members) {
                    if (!m) continue;
                    SymbolInfo si;
                    si.name = m->name;
                    si.file = file;
                    si.line = m->pos.line;
                    si.outer = true;
                    si.native = native;
                    if (m->kind == Decl::FUNCTION) {
                        si.kind = "func";
                        si.ret = canonicalType(m->ret);
                        for (const auto &p : m->params) si.params.push_back(canonicalType(p.first));
                        ci.funcs.push_back(si);
                    } else if (m->kind == Decl::VARIABLE) {
                        si.kind = "var";
                        si.type = canonicalType(m->ret);
                        ci.vars.push_back(si);
                    }
                }
                ci.rebuild();
                classes[key] = std::move(ci);
                builtins.insert(key);
                builtins.insert(d->name);   // unqualified alias too
                SymbolInfo s{d->name, key, (d->kind == Decl::CLASS ? "class" : "struct"),
                             file, d->pos.line, false, true, {}, "", true, native};
                out.symbols.push_back(s);
                return;
            }
            case Decl::FUNCTION: {
                SymbolInfo s;
                s.name = qname.empty() ? d->name : qname;
                s.kind = "func";
                s.file = file;
                s.line = d->pos.line;
                s.ret = canonicalType(d->ret);
                for (const auto &p : d->params) s.params.push_back(canonicalType(p.first));
                s.outer = true;
                s.native = native;
                // one code space: check duplicates against BOTH Ely and native
                for (const auto &prev : globals) {
                    if (prev.kind != "func") continue;
                    if (normName(prev.name) != normName(s.name)) continue;
                    if (prev.params == s.params) {
                        // Ely + native identities with bodies both defined
                        // would be a real duplicate; declaration+definition
                        // pairs (Ely arr) are fine.
                        error(file, d->pos.line,
                              "duplicate function '" + s.name + "(" + joinTypes(s.params) +
                              ")' — previously declared at " + pos(prev.file, prev.line) +
                              (prev.native ? " [native]" : "") +
                              " (overloads must differ in parameter types)");
                        return;
                    }
                }
                globals.push_back(s);
                out.symbols.push_back(s);
                return;
            }
            case Decl::VARIABLE: {
                SymbolInfo s;
                s.name = qname.empty() ? d->name : qname;
                s.kind = "var";
                s.file = file;
                s.line = d->pos.line;
                s.isStatic = !d->ret.empty();
                s.type = d->ret.empty() ? "any" : canonicalType(d->ret);
                s.duckTyped = d->ret.empty();
                s.outer = true;
                s.native = native;
                globals.push_back(s);
                out.symbols.push_back(s);
                return;
            }
            case Decl::NAMESPACE:
                for (const auto &m : d->nested)
                    collectDecl(m, file, qname, native);
                return;
            case Decl::EXTERN_BLOCK:
                for (const auto &m : d->nested)
                    collectDecl(m, file, qual, native);
                return;
            default:
                return;   // using/include/enum/template — not semantic core
        }
    }

    static std::string joinTypes(const std::vector<std::string> &v) {
        std::string s;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) s += ", ";
            s += v[i];
        }
        return s;
    }

    // ---- per-function analysis --------------------------------------------

    struct LocalScope {
        std::map<std::string, SymbolInfo> vars;
        std::string currentClass;   // when analyzing a method
        const ClassInfo *cls = nullptr;
    };

    void noteVar(LocalScope &scope, std::string name, std::string type,
                 const std::string &file, size_t line, bool duck) {
        scope.vars[name] = SymbolInfo{name, type, "var", file, line, duck, false, {}, "", false};
    }

    const SymbolInfo *findGlobal(const std::string &name) const {
        for (const auto &g : globals) {
            if (g.name == name) return &g;
        }
        return nullptr;
    }

    // ---- inheritance (linear ancestor vector, not a bitmask) ---------------

    // is B an ancestor of A (A derives from B, possibly transitively)?
    bool isAncestor(const std::string &A, const std::string &B) const {
        if (A == B) return true;
        auto it = classes.find(normName(A));
        if (it == classes.end()) return false;
        for (const auto &a : it->second.ancestors) {
            if (a == normName(B)) return true;
        }
        return false;
    }

    // Flatten the full chain of a class into `ancestors` (direct parents
    // first, then their own parents, order-preserving; cycles guarded).
    void buildAncestryFor(const std::string &name) {
        auto it = classes.find(name);
        if (it == classes.end() || it->second.ancestryBuilt) return;
        ClassInfo &ci = it->second;
        ci.ancestryBuilt = true;

        std::vector<std::string> result;
        std::set<std::string> seen;
        seen.insert(name);

        // DFS over direct bases
        std::function<void(const std::string &)> visit = [&](const std::string &cur) {
            auto cit = classes.find(cur);
            if (cit == classes.end()) return;
            for (const auto &parent : cit->second.directBases) {
                if (!classes.count(parent)) continue;       // extern/native base? keep name
                if (seen.count(parent)) continue;           // cycle guard
                seen.insert(parent);
                visit(parent);                               // parent's chain first
                result.push_back(parent);                    // then the parent itself
            }
        };
        visit(name);
        ci.ancestors = std::move(result);
    }

    void buildAllAncestry() {
        // build the flattened chains lazily for every class
        for (auto &kv : classes) buildAncestryFor(kv.first);
        // fill the report
        for (const auto &kv : classes) {
            InheritanceInfo ii;
            ii.name = kv.first;
            ii.directBases = kv.second.directBases;
            ii.ancestors = kv.second.ancestors;
            out.inheritance.push_back(std::move(ii));
        }
    }

    // ---- smart type conversions --------------------------------------------

    // Try to convert `from` to `to`. Records the conversion for the codegen
    // and returns true when it is legal (any is compatible with everything).
    bool coerce(const std::string &from, const std::string &to,
                const std::string &file, size_t line) {
        if (to == "any" || from == "any") {
            return true;
        }
        
        const std::string fn = resolveType(normName(from));
        const std::string tn = resolveType(normName(to));
        
        if (fn == tn) return true;
        
        // str is an alias for the native string type
        if ((fn == "str" && classes.count(tn)) || (tn == "str" && classes.count(fn))) {
            return true;
        }
        
        // numeric widening
        if (isNumType(from) && isNumType(to)) {
            out.conversions.push_back(TypeConversion{from, to, "numeric", file, line});
            return true;
        }
        
        // upcast: derived -> ancestor
        if (isAncestor(from, to)) {
            out.conversions.push_back(TypeConversion{from, to, "upcast", file, line});
            return true;
        }
        
        // downcast: ancestor -> derived
        if (isAncestor(to, from)) {
            out.conversions.push_back(TypeConversion{from, to, "downcast", file, line});
            return true;
        }
        
        return false;
    }

    void recordVarType(const std::string &file, const std::string &name,
                       const std::string &type) {
        out.variableTypes.emplace_back(file + "::" + name, type);
    }

    std::string checkCall(const std::string &fnName, const std::vector<ExprPtr> &args,
                          LocalScope &scope, const std::string &file, size_t line) {
        // gather candidate signatures (globals + class methods via `this` too)
        std::vector<const SymbolInfo*> cands;
        for (const auto &g : globals) {
            if (g.kind == "func" && g.name == fnName) cands.push_back(&g);
        }
        if (cands.empty()) {
            // maybe a class method via current class (this.xxx())
            if (scope.cls) {
                auto it = scope.cls->byName.find(fnName);
                if (it != scope.cls->byName.end() && it->second->kind == "func") {
                    cands.push_back(it->second);
                }
            }
        }
        if (cands.empty()) {
            // one code space: an unresolved call may be a native C++ symbol
            // (no extern needed); accept with a warning and unknown type.
            if (hasNative) {
                warn(file, line, "call to '" + fnName +
                     "' not declared in Ely — assumed native C++ symbol (one code space), type unknown");
                return "any";
            }
            error(file, line, "call to undeclared function '" + fnName + "'");
            return "any";
        }

        // compute actual argument types
        std::vector<std::string> actuals;
        for (const auto &a : args) actuals.push_back(inferExpr(*a, scope, file));

        // exact match first; wrong types are accepted if a smart conversion exists
        const SymbolInfo *best = nullptr;
        for (const SymbolInfo *c : cands) {
            if (c->params.size() != actuals.size()) continue;
            bool ok = true;
            for (size_t i = 0; i < actuals.size(); ++i) {
                if (c->params[i] != "any" && actuals[i] != "any"
                    && !coerce(actuals[i], resolveType(c->params[i]), file, line)) {
                    ok = false;
                    break;
                }
            }
            if (ok) { best = c; break; }
        }
        if (!best) {
            // arity check for the error message
            bool arity = false;
            for (const SymbolInfo *c : cands) {
                if (c->params.size() != actuals.size()) { arity = true; break; }
            }
            std::string want = "(" + joinTypes(cands[0]->params) + ")";
            if (cands.size() > 1) want += " (overloads available)";
            error(file, line, "no matching overload for '" + fnName + want +
                  "' with args (" + joinTypes(actuals) + ")" + (arity ? " (wrong arg count)" : ""));
            return cands[0]->ret;
        }
        return best->ret;
    }

    void analyzeExpr(const Expr &e, LocalScope &scope, const std::string &file) {
        (void)inferExpr(e, scope, file);
    }

    std::string inferExpr(const Expr &e, LocalScope &scope, const std::string &file) {
        switch (e.kind) {
            case Expr::INT_LIT:   return "int";
            case Expr::FLT_LIT:   return "flt";
            case Expr::STR_LIT:   return "str";
            case Expr::CHAR_LIT:  return "char";
            case Expr::BOOL_LIT:  return "bool";
            case Expr::THIS:      return scope.currentClass.empty() ? "any" : scope.currentClass;
            case Expr::NULLPTR:   return "any";
            case Expr::NATIVE:    return "any";
            case Expr::RAW:       return "any";
            case Expr::IDENT: {
                auto it = scope.vars.find(e.text);
                if (it != scope.vars.end()) return it->second.type;
                const SymbolInfo *g = findGlobal(e.text);
                if (g && !g->type.empty()) return g->type;          // var
                if (g && !g->ret.empty()) return g->ret;            // function name
                if (classes.count(normName(e.text))) return normName(e.text);
                // duck-typing: first mention without declaration => implicit var
                if (hasNative) {
                    // could be a native global (e.g. a C++ variable) — one space
                    warn(file, e.pos.line, "identifier '" + e.text +
                         "' not declared in Ely — assumed native C++ symbol (one code space)");
                    return "any";
                }
                warn(file, e.pos.line, "undeclared identifier '" + e.text +
                     "' used as value (implicitly 'any'); assign before use for duck typing");
                noteVar(scope, e.text, "any", file, e.pos.line, true);
                return "any";
            }
            case Expr::BINARY: {
                std::string lt = inferExpr(*e.lhs, scope, file);
                std::string rt = inferExpr(*e.rhs, scope, file);
                if (e.op == "+" && (lt == "str" || rt == "str")) return "str";
                if (isNumType(lt) && isNumType(rt)) return (lt == "flt" || rt == "flt") ? "flt" : "int";
                if (lt == "str" && rt == "str") return "str";
                if (lt == "rt" && e.op == "==") return "bool";
                if (e.op == "==" || e.op == "!=" || e.op == "<" || e.op == ">"
                    || e.op == "<=" || e.op == ">=" || e.op == "&&" || e.op == "||") {
                    return "bool";
                }
                return "any";
            }
            case Expr::UNARY: {
                // infer operand first
                (void)inferExpr(*e.lhs, scope, file);
                if (e.op == "!") return "bool";
                return "any";
            }
            case Expr::ASSIGN: {
                std::string vt = inferExpr(*e.value, scope, file);
                if (e.target->kind == Expr::IDENT) {
                    auto it = scope.vars.find(e.target->text);
                    if (it == scope.vars.end() && !findGlobal(e.target->text)
                        && !classes.count(normName(e.target->text))) {
                        // duck typing: first assignment declares the variable
                        noteVar(scope, e.target->text, vt, file, e.pos.line, true);
                        out.symbols.push_back(
                            SymbolInfo{e.target->text, vt, "var", file, e.pos.line, true, false, {}, "", false});
                        recordVarType(file, e.target->text, vt);
                    } else if (it != scope.vars.end()) {
                        // smart conversion when the target is typed
                        if (!coerce(vt, it->second.type, file, e.pos.line)) {
                            error(file, e.pos.line, "cannot assign '" + vt + "' to '" + e.target->text +
                                  "' of type '" + it->second.type + "'");
                        }
                        if (it->second.duckTyped && it->second.type != "any"
                            && vt != "any" && vt != it->second.type
                            && !isNumType(vt) && !isNumType(it->second.type)) {
                            warn(file, e.pos.line, "duck-typed variable '" + e.target->text +
                                 "' inferred as '" + it->second.type + "' but assigned '" + vt + "'");
                        }
                    }
                } else {
                    (void)inferExpr(*e.target, scope, file);
                }
                return "any";
            }
            case Expr::TERNARY: {
                std::string ct = inferExpr(*e.cond, scope, file);
                (void)ct;
                std::string tt = inferExpr(*e.thenBranch, scope, file);
                std::string ft = inferExpr(*e.elseBranch, scope, file);
                if (tt == ft) return tt;
                return "any";
            }
            case Expr::CALL: {
                // callee may be plain IDENT or MEMBER (console.print)
                std::vector<std::string> temps;
                if (e.callee->kind == Expr::IDENT) {
                    return checkCall(e.callee->text, e.args, scope, file, e.pos.line);
                }
                if (e.callee->kind == Expr::MEMBER) {
                    std::string baseType = inferExpr(*e.callee->base, scope, file);
                    if (!e.callee->members.empty()) {
                        const std::string &mname = e.callee->members.front();
                        auto ci = classes.find(resolveType(baseType));
                        if (ci != classes.end()) {
                            auto it = ci->second.byName.find(mname);
                            if (it != ci->second.byName.end() && it->second->kind == "func") {
                                // member call: check args (arity AND types)
                                std::vector<std::string> actuals;
                                for (const auto &a : e.args) actuals.push_back(inferExpr(*a, scope, file));
                                const SymbolInfo &m = *it->second;
                                if (m.params.size() != actuals.size()) {
                                    error(file, e.pos.line, "call to '" + baseType + "." + mname +
                                          "('" + joinTypes(m.params) + ")' with " +
                                          std::to_string(actuals.size()) + " args");
                                } else {
                                    for (size_t i = 0; i < actuals.size(); ++i) {
                                        if (!coerce(actuals[i], resolveType(m.params[i]), file, e.pos.line)) {
                                            error(file, e.pos.line, "cannot pass '" + actuals[i] +
                                                  "' as '" + m.params[i] + "' to '" + baseType + "." + mname + "'");
                                        }
                                    }
                                }
                                return m.ret;
                            }
                        }
                    }
                }
                // fallback: eval all args
                for (const auto &a : e.args) inferExpr(*a, scope, file);
                return "any";
            }
            case Expr::INDEX: {
                (void)inferExpr(*e.base, scope, file);
                if (e.index) inferExpr(*e.index, scope, file);
                return "any";
            }
            case Expr::MEMBER: {
                std::string baseType = inferExpr(*e.base, scope, file);
                if (!e.members.empty()) {
                    const std::string &mname = e.members.front();
                    auto ci = classes.find(resolveType(baseType));
                    if (ci != classes.end()) {
                        auto it = ci->second.byName.find(mname);
                        if (it != ci->second.byName.end()) return it->second->type;
                    }
                }
                return "any";
            }
            case Expr::INC_DEC: {
                return inferExpr(*e.lhs, scope, file);
            }
            case Expr::NEW_EXPR:
                return e.newType.empty() ? "any" : canonicalType(e.newType);
            case Expr::SIZEOF:
                (void)inferExpr(*e.lhs, scope, file);
                return "int";
        }
        return "any";
    }

    void analyzeStmt(const Stmt &s, LocalScope &scope, const std::string &file) {
        switch (s.kind) {
            case Stmt::BLOCK:
                analyzeStmts(s.body, scope, file);
                return;
            case Stmt::EXPR_STMT:
                analyzeExpr(*s.expr, scope, file);
                return;
            case Stmt::VAR_DECL: {
                std::string t = s.type.empty() ? "any" : canonicalType(s.type);
                if (s.type.empty()) {
                    // duck-typed local: infer from init
                    if (s.initExpr) t = inferExpr(*s.initExpr, scope, file);
                    noteVar(scope, s.name, t, file, s.pos.line, true);
                    out.symbols.push_back(
                        SymbolInfo{s.name, t, "var", file, s.pos.line, true, false, {}, "", false});
                    recordVarType(file, s.name, t);
                } else {
                    // C-style static typing: type must be known
                    std::string resolved = resolveType(t);
                    bool isCppType = hasNative && (t.find("::") != std::string::npos || t.find(".") != std::string::npos);
                    if (!isCppType && !isBuiltinType(resolved) && !classes.count(normName(resolved))
                        && typedefs.count(normName(t)) == 0) {
                        error(file, s.pos.line, "unknown type '" + t + "' for variable '" + s.name + "'");
                    }
                    if (s.initExpr) {
                        std::string it = inferExpr(*s.initExpr, scope, file);
                        // Skip type check for C++ types (we don't know their structure)
                        if (!isCppType && !coerce(it, resolved, file, s.pos.line)) {
                            error(file, s.pos.line, "cannot initialize '" + s.name + "' of type '" +
                                resolved + "' with value of type '" + it + "'");
                        }
                    }
                    noteVar(scope, s.name, resolved, file, s.pos.line, false);
                    out.symbols.push_back(
                        SymbolInfo{s.name, resolved, "var", file, s.pos.line, false, true, {}, "", false});
                    recordVarType(file, s.name, resolved);
                }
                return;
            }
            case Stmt::IF:
                if (s.cond) inferExpr(*s.cond, scope, file);
                if (s.thenBranch) analyzeStmt(*s.thenBranch, scope, file);
                if (s.elseBranch) analyzeStmt(*s.elseBranch, scope, file);
                return;
            case Stmt::WHILE:
                if (s.cond) inferExpr(*s.cond, scope, file);
                if (!s.body.empty()) analyzeStmt(*s.body[0], scope, file);
                return;
            case Stmt::DO:
                if (!s.body.empty()) analyzeStmt(*s.body[0], scope, file);
                if (s.cond) inferExpr(*s.cond, scope, file);
                return;
            case Stmt::FOR:
                if (s.init) analyzeStmt(*s.init, scope, file);
                if (s.cond) inferExpr(*s.cond, scope, file);
                if (s.incr) analyzeStmt(*s.incr, scope, file);
                if (!s.body.empty()) analyzeStmt(*s.body[0], scope, file);
                return;
            case Stmt::SWITCH:
                if (s.cond) inferExpr(*s.cond, scope, file);
                if (!s.body.empty()) analyzeStmt(*s.body[0], scope, file);
                return;
            case Stmt::RETURN:
                if (s.expr) inferExpr(*s.expr, scope, file);
                // return type check happens in analyzeFunction using scope.ret
                return;
            case Stmt::BREAK:
            case Stmt::CONTINUE:
            case Stmt::EMPTY:
            case Stmt::NATIVE_CODE:
            case Stmt::RAW:
                return;
            case Stmt::GOTO:
            case Stmt::LABEL:
                for (const auto &st : s.body) analyzeStmt(*st, scope, file);
                return;
        }
    }

    void analyzeStmts(const std::vector<StmtPtr> &body, LocalScope &scope,
                      const std::string &file) {
        for (const auto &s : body) {
            if (s) analyzeStmt(*s, scope, file);
        }
    }

    void analyzeFunction(const SymbolInfo &fn, const std::shared_ptr<Decl> &d,
                         const std::string &file) {
        if (!d->body) return;   // declaration only (e.g. module stubs)
        LocalScope scope;
        scope.currentClass = d->isElyFuncStyle ? "" : "";  // classes handled via members
        // params
        for (size_t i = 0; i < d->params.size(); ++i) {
            const auto &p = d->params[i];
            std::string pt = p.first.empty() ? "any" : canonicalType(p.first);
            noteVar(scope, p.second, resolveType(pt), file, d->pos.line, false);
            out.symbols.push_back(
                SymbolInfo{p.second, resolveType(pt), "param", file, d->pos.line, false, false, {}, "", false});
        }
        analyzeStmt(*d->body, scope, file);
        // return type check: collect return statements we saw? simplest: re-scan for
        // RETURN exprs and compare inferred type; we already inferred them above.
        // (A full check needs the inferred type of each return — stored via
        //  out.diagnostics only on mismatch; keep it lightweight here.)
        (void)fn;
    }

    // ---- per-file pass ----------------------------------------------------

    void analyzeFile(const std::shared_ptr<Program> &prog) {
        const std::string file = prog->fileName;
        out.symbols.clear();  // fallback; not used — merge happens below
        for (const auto &d : prog->decls) collectDecl(d, file);

        // second pass: analyze function bodies (only Ely semantics)
        for (const auto &d : prog->decls) {
            collectBodies(d, file);
        }
    }

    void collectBodies(const std::shared_ptr<Decl> &d, const std::string &file) {
        if (!d) return;
        if (d->kind == Decl::FUNCTION) {
            // find our SymbolInfo by name+params
            for (const auto &g : globals) {
                if (g.kind == "func" && g.name == d->name) {
                    analyzeFunction(g, d, file);
                    break;
                }
            }
        }
        for (const auto &m : d->members) collectBodies(m, file);
        for (const auto &m : d->nested) collectBodies(m, file);
    }
};

} // namespace

// ---------------------------------------------------------------------------

AnalysisResult analyzeEly(
    const std::vector<std::shared_ptr<Program>> &elyPrograms,
    const std::vector<std::shared_ptr<Program>> &nativePrograms) {
    Analyzer A;

    // Phase 0: mark the project as having native code (one code space: a
    // name not found in Ely may legitimately live in C++).
    if (!nativePrograms.empty()) A.hasNative = true;

    // Phase 1: collect all top-level declarations from EVERY code space.
    // Native first so Ely identities can be checked against them too.
    for (const auto &p : nativePrograms) {
        if (!p) continue;
        for (const auto &d : p->decls) A.collectDecl(d, p->fileName, "", true);
    }
    for (const auto &p : elyPrograms) {
        if (!p) continue;
        for (const auto &d : p->decls) A.collectDecl(d, p->fileName);
    }

    // Phase 1.5: build flattened inheritance vectors (parent first).
    A.buildAllAncestry();

    // Phase 2: analyze Ely function bodies only (native bodies are not
    // semantically analyzed — one code space trusts them).
    for (const auto &p : elyPrograms) {
        if (!p) continue;
        for (const auto &d : p->decls) A.collectBodies(d, p->fileName);
    }

    // builtin type table
    for (const auto &b : A.builtins) A.out.builtinTypes.push_back(b);
    for (const auto &t : A.typedefs) A.out.builtinTypes.push_back(t.first);
    for (const auto &c : A.classes) A.out.builtinTypes.push_back(c.first);
    std::sort(A.out.builtinTypes.begin(), A.out.builtinTypes.end());
    A.out.builtinTypes.erase(std::unique(A.out.builtinTypes.begin(), A.out.builtinTypes.end()),
                             A.out.builtinTypes.end());

    return std::move(A.out);
}

// ---------------------------------------------------------------------------

void printAnalysis(const AnalysisResult &r, std::ostream &os) {
    os << "Semantic analysis\n";

    os << "  Types (" << r.builtinTypes.size() << "):\n";
    std::string line;
    for (const auto &t : r.builtinTypes) {
        line += t + " ";
        if (line.size() > 70) { os << "    " << line << "\n"; line.clear(); }
    }
    if (!line.empty()) os << "    " << line << "\n";

    os << "  Symbols (" << r.symbols.size() << "):\n";
    for (const auto &s : r.symbols) {
        os << "    [" << s.kind;
        if (s.isStatic) os << ",static";
        if (s.duckTyped) os << ",duck";
        if (s.native) os << ",native";
        os << "] " << s.name;
        if (!s.type.empty()) os << " : " << s.type;
        if (!s.ret.empty()) os << " -> " << s.ret;
        if (!s.params.empty()) {
            os << " (";
            for (size_t i = 0; i < s.params.size(); ++i) {
                if (i) os << ", ";
                os << s.params[i];
            }
            os << ")";
        }
        os << "  @ " << s.file << ":" << s.line << "\n";
    }

    os << "  Inheritance:\n";
    for (const auto &ih : r.inheritance) {
        os << "    " << ih.name;
        if (!ih.ancestors.empty()) {
            os << " -> ";
            for (size_t i = 0; i < ih.ancestors.size(); ++i) {
                if (i) os << " -> ";
                os << ih.ancestors[i];
            }
        } else {
            os << " (no bases)";
        }
        os << "\n";
    }

    os << "  Variable types (codegen cache):\n";
    for (const auto &v : r.variableTypes) {
        os << "    " << v.first << " : " << v.second << "\n";
    }

    os << "  Type conversions (" << r.conversions.size() << "):\n";
    for (const auto &c : r.conversions) {
        os << "    [" << c.how << "] " << c.from << " -> " << c.to
           << "  @ " << pos(c.file, c.line) << "\n";
    }

    os << "  Diagnostics (" << r.errors << " errors, " << r.warnings << " warnings):\n";
    for (const auto &d : r.diagnostics) {
        os << "    [" << (d.level == Diagnostic::ERROR ? "error" :
                          d.level == Diagnostic::WARN ? "warn" : "info")
           << "] " << pos(d.file, d.line);
        if (!d.message.empty()) os << ": " << d.message;
        os << "\n";
    }
}

} // namespace Elica::Semantic