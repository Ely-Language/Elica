// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>

#include "parser/earley_things.h"
#include "features/macro.h"

namespace Elica::Parsing::Earley::Rules {

enum class GrammarType {
EXPRESSION,
STATEMENT,
MODIFIER
};

class RuleSet {
private:
    // by grammar type
    std::unordered_map<GrammarType, std::vector<::Elica::Parsing::Earley::Things::Rule>> rulesByGrammar;

    // all rules
    std::vector<::Elica::Parsing::Earley::Things::Rule> allRules;

    GrammarType targetTypeToGrammar(const std::string &targetType) const {
        if (targetType == "modifier") return GrammarType::MODIFIER;
        if (targetType == "statement") return GrammarType::STATEMENT;
        if (targetType == "expression") return GrammarType::EXPRESSION;
        return GrammarType::EXPRESSION;
    }
public:
    
    void addRule(std::string left, std::vector<std::string> right) {
        
    }


};

}