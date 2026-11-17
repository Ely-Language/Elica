// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "parser/ast_nodes.h"

namespace Elica::Semantic {

// ---------------------------------------------------------------------------
// Semantic analysis for Ely.
//
// Ely and C++ live in ONE code space: C++ functions/classes/types from
// native modules (.h/.cpp/.hpp, cppCode blocks) are callable from Ely
// directly — no extern/FFI declarations needed. Therefore the analyzer
// merges declarations from native programs into the same symbol/type tables.
//
// Scope:
//   * collects declarations: functions, classes/structs, typedefs, variables
//     (both explicit `int x;` — C-style static typing — and duck-typed ones)
//     from Ely AND native (C/C++) programs;
//   * builds type tables (builtins + user types) and resolves TypeRefs;
//   * checks duplicate implementations of functions/classes, treating
//     overloads (same name, different parameter types) as legal;
//   * does duck typing: a bare `x = expr;` (no keywords, no declared type)
//     declares `x` with the inferred type of the RHS; explicit `any` works
//     the same;
//   * checks usage: undeclared identifiers, calls (args count / overloads).
//     If a name is not found but the project contains native code, the call
//     is accepted as a direct C++ symbol (one code space) with a warning,
//     and the type is treated as unknown ("any").
// C/C++ file BODIES are not analyzed — only their declarations are merged.
// ---------------------------------------------------------------------------

struct SymbolInfo {
    std::string name;
    std::string type;                 // canonical type name ("int", "flt", ...)
    std::string kind;                 // "func" | "var" | "class" | "typedef" | "param"
    std::string file;
    size_t line = 0;
    bool duckTyped = false;           // declared by first assignment, no type
    bool isStatic = false;            // explicit C-style type in declaration
    std::vector<std::string> params;  // func: parameter types (for overloads)
    std::string ret;                  // func: return type
    bool outer = false;               // symbol visible from other files (module)
    bool native = false;              // defined in C/C++ (same code space)
};

struct Diagnostic {
    enum Level { INFO, WARN, ERROR } level;
    std::string file;
    size_t line = 0;
    std::string message;
};

// One smart type conversion recorded during analysis: the code generator can
// emit its own conversion (numeric promotion, upcast along the inheritance
// vector, implicit str<->any, ...) instead of failing or guessing.
struct TypeConversion {
    std::string from;
    std::string to;
    std::string how;                 // "numeric" | "upcast" | "downcast" | "implicit"
    std::string file;
    size_t line = 0;
};

// Full inheritance chain of a class (already resolved, no bitmask): each
// entry is the name of an ancestor. directBases are the declared parents;
// ancestors is the flattened, order-preserving vector (parent first).
struct InheritanceInfo {
    std::string name;
    std::vector<std::string> directBases;
    std::vector<std::string> ancestors;
};

struct AnalysisResult {
    std::vector<SymbolInfo> symbols;      // full symbol table (files merged)
    std::vector<std::string> builtinTypes; // all known type names
    std::vector<Diagnostic> diagnostics;
    std::vector<TypeConversion> conversions;
    std::vector<InheritanceInfo> inheritance;   // precomputed lineages
    // resolved variable types for the codegen cache: name -> canonical type
    // (last winning decl wins; locals are file-qualified: "file::name")
    std::vector<std::pair<std::string, std::string>> variableTypes;
    size_t errors = 0;
    size_t warnings = 0;
};

// Runs semantic analysis over one or more parsed Ely files (e.g. the whole
// project). `nativePrograms` are parsed C/C++ files whose DECLARATIONS are
// merged into the same code space (their bodies are not analyzed).
AnalysisResult analyzeEly(
    const std::vector<std::shared_ptr<::Elica::Parsing::Program>> &elyPrograms,
    const std::vector<std::shared_ptr<::Elica::Parsing::Program>> &nativePrograms = {});

// Pretty-prints the report (symbol table, types, diagnostics).
void printAnalysis(const AnalysisResult &result, std::ostream &os);

} // namespace Elica::Semantic