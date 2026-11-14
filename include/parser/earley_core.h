// SPDX-License-Identifier: MIT
#pragma once

// Token-based Earley parser with parse-tree extraction.
//
// Grammar symbols are strings. A symbol that never appears as a left-hand
// side is a TERMINAL; terminals are matched against lexer tokens:
//   "id"             -> IDENTIFIER
//   "num"            -> INT_LITERAL | FLT_LITERAL
//   "str"            -> STR/F_STR/MULTILINE literals
//   "char"           -> CHAR_LITERAL
//   "cpp_code"       -> CommonToken::NATIVE_CODE (collapsed cppCode/cCode block)
//   anything else    -> token.lexeme == symbol  (keywords and operators)
//
// Chart states are deduplicated by (rule, dot, start); the first derivation
// wins (collapsed shared packed forest), which keeps expression ambiguity
// from exploding while still producing deterministic parse trees.

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lexer/token.h"
#include "parser/earley_things.h"

namespace Elica::Parsing::Earley {

using Symbol = std::string;

// ---------------------------------------------------------------------------
// Grammar
// ---------------------------------------------------------------------------

class Grammar {
public:
    Symbol start;
    std::vector<::Elica::Parsing::Earley::Things::Rule> rules;
    std::unordered_map<Symbol, std::vector<int>> byLhs;
    std::unordered_set<Symbol> nonTerminals;

    void addRule(const Symbol &lhs, std::vector<Symbol> rhs) {
        rules.emplace_back(lhs, std::move(rhs));
        byLhs[lhs].push_back(static_cast<int>(rules.size()) - 1);
        nonTerminals.insert(lhs);
    }

    // A symbol is a terminal when it is never a left-hand side.
    bool isTerminal(const Symbol &s) const { return nonTerminals.count(s) == 0; }
};

// ---------------------------------------------------------------------------
// Parse tree
// ---------------------------------------------------------------------------

struct ParseNode {
    Symbol symbol;
    int tokenIndex = -1;                         // terminal: index into input
    const ::Elica::Parsing::Earley::Things::Rule *rule = nullptr; // nonterminal
    std::vector<std::shared_ptr<ParseNode>> children;
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

class EarleyParser {
public:
    // Parses `tokens` (term "END_OF_FILE" must NOT be present) starting from
    // grammar.start. Returns the parse tree, or nullopt when the input does
    // not derive the start symbol / the work limit is exceeded.
    std::optional<std::shared_ptr<ParseNode>> parse(
        const std::vector<::Elica::Lexer::Token> &tokens,
        const Grammar &g,
        size_t maxOps = 4'000'000);

private:
    struct Item {
        const ::Elica::Parsing::Earley::Things::Rule *rule;
        size_t dot;
        size_t start;
        std::vector<std::shared_ptr<ParseNode>> children; // one per consumed RHS symbol
    };

    const ::Elica::Lexer::Token *tokAt(
        const std::vector<::Elica::Lexer::Token> &tokens, size_t i) const {
        return (i < tokens.size()) ? &tokens[i] : nullptr;
    }

    bool matches(const ::Elica::Lexer::Token &tok, const Symbol &term) const;
};

} // namespace Elica::Parsing::Earley