// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "lexer/token.h"
#include "core/utils.h"

using namespace std;

namespace Elica::Lexer {

class Lexer {
private:
    string source;
    size_t pos = 0;
    size_t line = 1;
    size_t column = 1;
    LexerLanguage language;

    // get current symbol
    char peek() const;

    // get current symbol & move to next
    char advance();

    // is end of file
    bool isAtEnd();

    // is current symbol expected
    bool match(char expected);

    void skipWhitespaceAndComments();
    Token scanNumber();
    Token scanIdentifierOrKeyword();
    Token scanString();

    const unordered_map<string, TokenType>& getElyKeywords();
    const unordered_map<string, TokenType>& getCKeywords();
    const unordered_map<string, TokenType>& getCppKeywords();

public:
    Lexer(string src, LexerLanguage lang);
    // resets Lexer for next file
    inline void setfull(string src, LexerLanguage lang) {
        this->source=src; this->language=lang;
        this->pos=0; this->line=1; this->column=1;
    }
    // tokenizes the current file
    vector<Token> tokenize();
};

}