// SPDX-License-Identifier: MIT
// Token-based Earley parser with parse-tree extraction.
//
// Grammar symbols are strings. A symbol that never appears as a left-hand
// side is a TERMINAL; terminals are matched against lexer tokens:
//   "id"         -> IDENTIFIER          "num" -> INT/FLT literals
//   "str_lit"    -> string literals     "char_lit" -> CHAR_LITERAL
//   "cpp_code"   -> CommonToken::NATIVE_CODE (collapsed cCode/cppCode block)
//   otherwise    -> token.lexeme == symbol (keywords and operators)
//
// Implementation notes (classic Earley + ε-robustness):
//   * chart[i].byExpected[X] lists items in state i whose dot sits on the
//     nonterminal X; byCompleted[X] lists completed X items in state i.
//   * When an item that expects X is inserted, it is immediately advanced
//     past every X already completed at that state (this makes ε-rules and
//     late predictions work regardless of creation order).
//   * The completer runs in the main loop: a completed item advances every
//     candidate awaiting its lhs found in chart[start]. Lists are copied
//     before iterating because the indexes grow while completing.
// Chart states are deduplicated by (rule, dot, start): the first derivation
// wins, keeping expression ambiguity from exploding while still producing
// deterministic parse trees.

#include "parser/earley_core.h"

#include <iostream>

namespace Elica::Parsing::Earley {

namespace {

bool isIdentToken(const ::Elica::Lexer::Token &t) {
    return std::holds_alternative<::Elica::Lexer::CommonToken>(t.type)
        && std::get<::Elica::Lexer::CommonToken>(t.type)
               == ::Elica::Lexer::CommonToken::IDENTIFIER;
}

bool isNumToken(const ::Elica::Lexer::Token &t) {
    if (!std::holds_alternative<::Elica::Lexer::CommonToken>(t.type)) return false;
    const auto ct = std::get<::Elica::Lexer::CommonToken>(t.type);
    return ct == ::Elica::Lexer::CommonToken::INT_LITERAL
        || ct == ::Elica::Lexer::CommonToken::FLT_LITERAL;
}

bool isStrToken(const ::Elica::Lexer::Token &t) {
    if (!std::holds_alternative<::Elica::Lexer::CommonToken>(t.type)) return false;
    const auto ct = std::get<::Elica::Lexer::CommonToken>(t.type);
    return ct == ::Elica::Lexer::CommonToken::STR_LITERAL
        || ct == ::Elica::Lexer::CommonToken::F_STR_LITERAL
        || ct == ::Elica::Lexer::CommonToken::MULTILINE_STR_LITERAL
        || ct == ::Elica::Lexer::CommonToken::F_MULTILINE_STR_LITERAL;
}

bool isCharToken(const ::Elica::Lexer::Token &t) {
    return std::holds_alternative<::Elica::Lexer::CommonToken>(t.type)
        && std::get<::Elica::Lexer::CommonToken>(t.type)
               == ::Elica::Lexer::CommonToken::CHAR_LITERAL;
}

bool isNativeToken(const ::Elica::Lexer::Token &t) {
    return std::holds_alternative<::Elica::Lexer::CommonToken>(t.type)
        && std::get<::Elica::Lexer::CommonToken>(t.type)
               == ::Elica::Lexer::CommonToken::NATIVE_CODE;
}

} // namespace

bool EarleyParser::matches(const ::Elica::Lexer::Token &tok,
                           const Symbol &term) const {
    if (term == "id")        return isIdentToken(tok);
    if (term == "num")       return isNumToken(tok);
    if (term == "str_lit")   return isStrToken(tok);
    if (term == "char_lit")  return isCharToken(tok);
    if (term == "cpp_code")  return isNativeToken(tok);
    return tok.lexeme == term;
}

std::optional<std::shared_ptr<ParseNode>> EarleyParser::parse(
    const std::vector<::Elica::Lexer::Token> &tokens,
    const Grammar &g,
    size_t maxOps) {
    const size_t n = tokens.size();

    struct EState {
        std::unordered_map<size_t, Item> items;   // node-based: Item* stable
        std::vector<const Item*> queue;
        std::unordered_map<Symbol, std::vector<const Item*>> byExpected;
        std::unordered_map<Symbol, std::vector<const Item*>> byCompleted;
    };
    std::vector<EState> chart(n + 1);

    const auto itemKey = [](int ruleIdx, size_t dot, size_t start) {
        return (static_cast<uint64_t>(ruleIdx) << 44)
             | (static_cast<uint64_t>(dot) << 22)
             | static_cast<uint64_t>(start);
    };

    std::function<bool(size_t, int, size_t, size_t,
                       std::vector<std::shared_ptr<ParseNode>>)> addItem;

    auto advancePast = [&](size_t state, const Item *cand,
                           const std::shared_ptr<ParseNode> &node) {
        auto children = cand->children;
        children.push_back(node);
        addItem(state, static_cast<int>(cand->rule - g.rules.data()),
                cand->dot + 1, cand->start, std::move(children));
    };

    addItem = [&](size_t state, int ruleIdx, size_t dot, size_t start,
                  std::vector<std::shared_ptr<ParseNode>> children) -> bool {
        auto &st = chart[state];
        const auto key = itemKey(ruleIdx, dot, start);
        if (st.items.count(key)) return false;          // dedup: first wins
        auto res = st.items.emplace(key,
            Item{&g.rules[ruleIdx], dot, start, std::move(children)});
        const Item *it = &res.first->second;
        st.queue.push_back(it);

        const auto &rhs = g.rules[ruleIdx].rhs;
        if (dot < rhs.size() && !g.isTerminal(rhs[dot])) {
            st.byExpected[rhs[dot]].push_back(it);
            // ε-robustness: if X already completed here, advance now (copy of
            // the list: it may grow while we iterate via nested addItem).
            auto comp = st.byCompleted.find(rhs[dot]);
            if (comp != st.byCompleted.end() && !comp->second.empty()) {
                auto compList = comp->second;
                for (const Item *C : compList) {
                    auto node = std::make_shared<ParseNode>();
                    node->symbol = rhs[dot];
                    node->rule = C->rule;
                    node->children = C->children;
                    advancePast(state, it, node);
                }
            }
        } else if (dot == rhs.size()) {
            st.byCompleted[g.rules[ruleIdx].lhs].push_back(it);
            // main-loop completer will advance waiters via chart[start];
            // nothing else to do on insertion.
        }
        return true;
    };

    size_t ops = 0;

    // Seed: every start rule at state 0.
    {
        const auto it = g.byLhs.find(g.start);
        if (it == g.byLhs.end()) return std::nullopt;
        for (const int r : it->second) addItem(0, r, 0, 0, {});
    }

    for (size_t i = 0; i <= n; ++i) {
        for (size_t qi = 0; qi < chart[i].queue.size(); ++qi) {
            if (ops++ >= maxOps) return std::nullopt;
            const Item &item = *chart[i].queue[qi];

            const auto &rhs = item.rule->rhs;
            if (item.dot < rhs.size()) {
                const Symbol &expect = rhs[item.dot];

                if (g.isTerminal(expect)) {
                    // ---- SCANNER
                    if (i < n && matches(tokens[i], expect)) {
                        auto leaf = std::make_shared<ParseNode>();
                        leaf->symbol = expect;
                        leaf->tokenIndex = static_cast<int>(i);
                        auto children = item.children;
                        children.push_back(std::move(leaf));
                        addItem(i + 1, static_cast<int>(item.rule - g.rules.data()),
                                item.dot + 1, item.start, std::move(children));
                    }
                } else {
                    // ---- PREDICTOR
                    const auto sub = g.byLhs.find(expect);
                    if (sub != g.byLhs.end()) {
                        for (const int sr : sub->second) {
                            if (ops++ >= maxOps) return std::nullopt;
                            addItem(i, sr, 0, i, {});
                        }
                    }
                }
            } else {
                // ---- COMPLETER: [A -> X• , start] at state i
                const Symbol &lhs = item.rule->lhs;
                auto node = std::make_shared<ParseNode>();
                node->symbol = lhs;
                node->rule = item.rule;
                node->children = item.children;

                auto ex = chart[item.start].byExpected.find(lhs);
                if (ex != chart[item.start].byExpected.end() && !ex->second.empty()) {
                    auto exList = ex->second;          // copy: may grow while advancing
                    for (const Item *cand : exList) {
                        if (ops++ >= maxOps) return std::nullopt;
                        advancePast(i, cand, node);
                    }
                }
            }
        }
    }

    // Acceptance: a start-rule item completed at the last state.
    const auto &accept = chart[n].items;
    for (const auto &[key, item] : accept) {
        (void)key;
        if (item.rule->lhs == g.start && item.dot == item.rule->rhs.size()
            && item.start == 0) {
            auto root = std::make_shared<ParseNode>();
            root->symbol = g.start;
            root->rule = item.rule;
            root->children = item.children;
            return root;
        }
    }
    // debug: completed items at the last state
    (void)accept;

    // Diagnostic on failure.
    size_t lastNonEmpty = 0;
    for (size_t i = 0; i <= n; ++i) {
        if (!chart[i].items.empty()) lastNonEmpty = i;
    }
    std::cerr << "[Elica][PARSE][ERROR] failed: tokens=" << n
              << ", last non-empty state=" << lastNonEmpty;
    if (lastNonEmpty < n) {
        const auto &tk = tokens[lastNonEmpty];
        std::cerr << ", unexpected '" << tk.lexeme << "' ("
                  << ::Elica::Lexer::tokenTypeName(tk) << ") at "
                  << tk.position.line << ":" << tk.position.column;
    }
    std::cerr << "\n";
    return std::nullopt;
}

} // namespace Elica::Parsing::Earley