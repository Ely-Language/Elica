// SPDX-License-Identifier: MIT
#pragma once

#include "parser/earley_core.h"

namespace Elica::Parsing {

// Pre-built grammars for Ely, C and C++.
// Terminals are matched by token lexeme / literal class (see EarleyParser).
::Elica::Parsing::Earley::Grammar buildElyGrammar();
::Elica::Parsing::Earley::Grammar buildCGrammar();
::Elica::Parsing::Earley::Grammar buildCppGrammar();

} // namespace Elica::Parsing