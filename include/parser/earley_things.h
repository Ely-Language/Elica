// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <memory>

#include "features/macro.h"

using namespace std;

namespace Elica::Parsing::Earley::Things {

struct Rule {
    string lhs;
    vector<string> rhs;
    shared_ptr<::Elica::Features::Preprocess::Macro::StructMacro> macroRef;

    // Base
    Rule(
        string _lhs,
        vector<string> _rhs
    ) : lhs(move(_lhs)), rhs(move(_rhs)), macroRef(nullptr) {}

    // For macros
    Rule(
        string _lhs,
        vector<string> _rhs,
        shared_ptr<::Elica::Features::Preprocess::Macro::StructMacro> macro
    ) : lhs(move(_lhs)), rhs(move(_rhs)), macroRef(move(macro)) {}
};

struct Item {
    const Rule* rule;
    size_t dotIndex;
    size_t startPosition;
    vector<const Item*> backPointers;

    Item(
        const Rule* _rule,
        size_t _dotIndex,
        size_t _startPosition,
        vector<const Item*> _backPointers = {}
    ) : rule(_rule), 
        dotIndex(_dotIndex), 
        startPosition(_startPosition), 
        backPointers(move(_backPointers)) {}
};

}