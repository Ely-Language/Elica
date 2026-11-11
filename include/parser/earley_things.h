// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <memory>

#include "features/macro.h"



namespace Elica::Parsing::Earley::Things {

struct Rule {
    std::string lhs;
    std::vector<std::string> rhs;
    std::shared_ptr<::Elica::Features::Preprocess::Macro::StructMacro> macroRef;

    // Base
    Rule(
        std::string _lhs,
        std::vector<std::string> _rhs
    ) : lhs(move(_lhs)), rhs(move(_rhs)), macroRef(nullptr) {}

    // For macros
    Rule(
        std::string _lhs,
        std::vector<std::string> _rhs,
        std::shared_ptr<::Elica::Features::Preprocess::Macro::StructMacro> macro
    ) : lhs(move(_lhs)), rhs(move(_rhs)), macroRef(move(macro)) {}
};

struct Item {
    const Rule* rule;
    size_t dotIndex;
    size_t startPosition;
    std::vector<const Item*> backPointers;

    Item(
        const Rule* _rule,
        size_t _dotIndex,
        size_t _startPosition,
        std::vector<const Item*> _backPointers = {}
    ) : rule(_rule), 
        dotIndex(_dotIndex), 
        startPosition(_startPosition), 
        backPointers(std::move(_backPointers)) {}
};

}