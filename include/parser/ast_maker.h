// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lexer/token.h"
#include "lexer/lexer.h"
#include "parser/ast_nodes.h"
#include "parser/earley_core.h"

namespace Elica::Parsing {

// ---------------------------------------------------------------------------
// Token pre-processing before parsing
// ---------------------------------------------------------------------------

// Ely: collapse `cCode { ... }` / `cppCode { ... }` blocks (produced by the
// lexer as a brace-wrapped token run) into single NATIVE_CODE tokens.
// The collapsed token's lexeme carries a marker plus the reconstructed raw
// block text. Blocks are reported via `langs` ("c"/"cpp" per block).
std::vector<::Elica::Lexer::Token> collapseNativeBlocks(
    const std::vector<::Elica::Lexer::Token> &tokens,
    std::vector<std::string> &langs);

// C/C++: pull preprocessor directive groups (lines starting with '#',
// detected by column == 1) out of the token stream. The remaining stream is
// parser-ready; directive lines are returned verbatim (reconstructed).
std::vector<::Elica::Lexer::Token> extractDirectives(
    const std::vector<::Elica::Lexer::Token> &tokens,
    std::vector<std::string> &directives);

// ---------------------------------------------------------------------------
// Parse driver
// ---------------------------------------------------------------------------

// Parses `tokens` with the grammar of `lang`. Returns nullptr on failure.
std::shared_ptr<::Elica::Parsing::Earley::ParseNode> parseTokens(
    const std::vector<::Elica::Lexer::Token> &tokens,
    ::Elica::Lexer::LexerLanguage lang);

// Builds the AST for one file from its parse tree.
std::shared_ptr<Program> buildProgram(
    const ::Elica::Parsing::Earley::ParseNode &tree,
    const std::vector<::Elica::Lexer::Token> &tokens,
    const std::string &fileName);

// Human-readable AST dump (used by `elica dev`).
void printProgram(const Program &prog, std::ostream &os);

} // namespace Elica::Parsing