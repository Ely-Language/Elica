// SPDX-License-Identifier: MIT
#include "lexer/lexer.h"
#include <cctype>
#include <iostream>

using namespace std;
namespace fs = ::std::filesystem;

namespace Elica::Lexer {

Lexer::Lexer(string src, LexerLanguage lang)
    : source(move(src)), language(lang) {}

char Lexer::peek() const {
    if (pos >= source.size()) return '\0';
    return source[pos];
}

char Lexer::advance() {
    char c = source[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

bool Lexer::isAtEnd() {
    return pos >= source.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[pos] != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance();
        } else if (c == '/'  && pos + 1 < source.size()  && source[pos + 1] == '/') {
            // Однострочный комментарий //
            while (!isAtEnd()  && peek() != '\n') advance();
        } else if (c == '/'  && pos + 1 < source.size()  && source[pos + 1] == '*') {
            // Многострочный комментарий /* ... */
            advance(); advance();
            while (!isAtEnd()) {
                if (peek() == '*'  && pos + 1 < source.size()  && source[pos + 1] == '/') {
                    advance(); advance();
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

// Ely
const unordered_map<string, TokenType> &Lexer::getElyKeywords() {
    static const unordered_map<string, TokenType> keywords = {
        // Types
        {"byte", static_cast<TokenType>(ElyToken::KW_BYTE)},
        {"ubyte", static_cast<TokenType>(ElyToken::KW_UBYTE)},
        {"int", static_cast<TokenType>(ElyToken::KW_INT)},
        {"uint", static_cast<TokenType>(ElyToken::KW_UINT)},
        {"short", static_cast<TokenType>(ElyToken::KW_SHORT)},
        {"ushort", static_cast<TokenType>(ElyToken::KW_USHORT)},
        {"more", static_cast<TokenType>(ElyToken::KW_MORE)},
        {"umore", static_cast<TokenType>(ElyToken::KW_UMORE)},
        {"flt", static_cast<TokenType>(ElyToken::KW_FLT)},
        {"double", static_cast<TokenType>(ElyToken::KW_DOUBLE)},
        {"noised", static_cast<TokenType>(ElyToken::KW_NOISED)},
        {"str", static_cast<TokenType>(ElyToken::KW_STR)},
        {"atom", static_cast<TokenType>(ElyToken::KW_ATOM)},
        {"char", static_cast<TokenType>(ElyToken::KW_CHAR)},
        {"bool", static_cast<TokenType>(ElyToken::KW_BOOL)},
        {"void", static_cast<TokenType>(ElyToken::KW_VOID)},
        {"sentinel", static_cast<TokenType>(ElyToken::KW_SENTINEL)},
        {"any", static_cast<TokenType>(ElyToken::KW_ANY)},

        // Def
        {"func", static_cast<TokenType>(ElyToken::KW_FUNC)},
        {"class", static_cast<TokenType>(ElyToken::KW_CLASS)},
        {"struct", static_cast<TokenType>(ElyToken::KW_STRUCT)},
        {"cCode", static_cast<TokenType>(ElyToken::KW_cCode)},
        {"cppCode", static_cast<TokenType>(ElyToken::KW_cppCode)},

        // Modifiers
        {"public", static_cast<TokenType>(ElyToken::KW_PUBLIC)},
        {"private", static_cast<TokenType>(ElyToken::KW_PRIVATE)},
        {"mine", static_cast<TokenType>(ElyToken::KW_MINE)},
        {"static", static_cast<TokenType>(ElyToken::KW_STATIC)},
        {"copa", static_cast<TokenType>(ElyToken::KW_COPA)},
        {"const", static_cast<TokenType>(ElyToken::KW_CONST)},
        {"nocopa", static_cast<TokenType>(ElyToken::KW_NOCOPA)},
        {"sealed", static_cast<TokenType>(ElyToken::KW_SEALED)},
        {"abstract", static_cast<TokenType>(ElyToken::KW_ABSTRACT)},
        {"wait", static_cast<TokenType>(ElyToken::KW_WAIT)},
        {"unwait", static_cast<TokenType>(ElyToken::KW_UNWAIT)},
        {"stack", static_cast<TokenType>(ElyToken::KW_STACK)},
        {"heap", static_cast<TokenType>(ElyToken::KW_HEAP)},

        // Flow
        {"if", static_cast<TokenType>(ElyToken::KW_IF)},
        {"else", static_cast<TokenType>(ElyToken::KW_ELSE)},
        {"while", static_cast<TokenType>(ElyToken::KW_WHILE)},
        {"for", static_cast<TokenType>(ElyToken::KW_FOR)},
        {"return", static_cast<TokenType>(ElyToken::KW_RETURN)},
        {"try", static_cast<TokenType>(ElyToken::KW_TRY)},
        {"raise", static_cast<TokenType>(ElyToken::KW_RAISE)},
        {"except", static_cast<TokenType>(ElyToken::KW_EXCEPT)},
        {"break", static_cast<TokenType>(ElyToken::KW_BREAK)},
        {"continue", static_cast<TokenType>(ElyToken::KW_CONTINUE)},

        // Logic  &moduling
        {"using", static_cast<TokenType>(ElyToken::KW_USING)},
        {"true", static_cast<TokenType>(ElyToken::KW_TRUE)},
        {"false", static_cast<TokenType>(ElyToken::KW_FALSE)},
        {"is", static_cast<TokenType>(ElyToken::KW_IS)},
        {"in", static_cast<TokenType>(ElyToken::KW_IN)},
        {"lambda", static_cast<TokenType>(ElyToken::KW_LAMBDA)},
        {"not", static_cast<TokenType>(ElyToken::KW_NOT)},
        {"this",  static_cast<TokenType>(ElyToken::KW_THIS)}
    };
    return keywords;
}

// C
const unordered_map<string, TokenType> &Lexer::getCKeywords() {
    static const unordered_map<string, TokenType> keywords = {
        {"auto", static_cast<TokenType>(CToken::KW_AUTO)},
        {"break", static_cast<TokenType>(CToken::KW_BREAK)},
        {"case", static_cast<TokenType>(CToken::KW_CASE)},
        {"char", static_cast<TokenType>(CToken::KW_CHAR)},
        {"const", static_cast<TokenType>(CToken::KW_CONST)},
        {"continue", static_cast<TokenType>(CToken::KW_CONTINUE)},
        {"default", static_cast<TokenType>(CToken::KW_DEFAULT)},
        {"do", static_cast<TokenType>(CToken::KW_DO)},
        {"double", static_cast<TokenType>(CToken::KW_DOUBLE)},
        {"else", static_cast<TokenType>(CToken::KW_ELSE)},
        {"enum", static_cast<TokenType>(CToken::KW_ENUM)},
        {"extern", static_cast<TokenType>(CToken::KW_EXTERN)},
        {"float", static_cast<TokenType>(CToken::KW_FLOAT)},
        {"for", static_cast<TokenType>(CToken::KW_FOR)},
        {"goto", static_cast<TokenType>(CToken::KW_GOTO)},
        {"if", static_cast<TokenType>(CToken::KW_IF)},
        {"inline", static_cast<TokenType>(CToken::KW_INLINE)},
        {"int", static_cast<TokenType>(CToken::KW_INT)},
        {"long", static_cast<TokenType>(CToken::KW_LONG)},
        {"register", static_cast<TokenType>(CToken::KW_REGISTER)},
        {"restrict", static_cast<TokenType>(CToken::KW_RESTRICT)},
        {"return", static_cast<TokenType>(CToken::KW_RETURN)},
        {"short", static_cast<TokenType>(CToken::KW_SHORT)},
        {"signed", static_cast<TokenType>(CToken::KW_SIGNED)},
        {"sizeof", static_cast<TokenType>(CToken::KW_SIZEOF)},
        {"static", static_cast<TokenType>(CToken::KW_STATIC)},
        {"struct", static_cast<TokenType>(CToken::KW_STRUCT)},
        {"switch", static_cast<TokenType>(CToken::KW_SWITCH)},
        {"typedef", static_cast<TokenType>(CToken::KW_TYPEDEF)},
        {"union", static_cast<TokenType>(CToken::KW_UNION)},
        {"unsigned", static_cast<TokenType>(CToken::KW_UNSIGNED)},
        {"void", static_cast<TokenType>(CToken::KW_VOID)},
        {"volatile", static_cast<TokenType>(CToken::KW_VOLATILE)},
        {"while", static_cast<TokenType>(CToken::KW_WHILE)},
        {"alignas", static_cast<TokenType>(CToken::KW_ALIGNAS)},
        {"alignof", static_cast<TokenType>(CToken::KW_ALIGNOF)},
        {"atomic", static_cast<TokenType>(CToken::KW_ATOMIC)},
        {"bool", static_cast<TokenType>(CToken::KW_BOOL)},
        {"complex", static_cast<TokenType>(CToken::KW_COMPLEX)},
        {"generic", static_cast<TokenType>(CToken::KW_GENERIC)},
        {"imaginary", static_cast<TokenType>(CToken::KW_IMAGINARY)},
        {"noreturn", static_cast<TokenType>(CToken::KW_NORETURN)},
        {"static_assert", static_cast<TokenType>(CToken::KW_STATIC_ASSERT)},
        {"thread_local", static_cast<TokenType>(CToken::KW_THREAD_LOCAL)},
        {"false", static_cast<TokenType>(CToken::KW_FALSE)},
        {"true", static_cast<TokenType>(CToken::KW_TRUE)}
    };
    return keywords;
}

// C++
const unordered_map<string, TokenType> &Lexer::getCppKeywords() {
    static const unordered_map<string, TokenType> keywords = {
        {"alignas", static_cast<TokenType>(CppToken::KW_ALIGNAS)},
        {"alignof", static_cast<TokenType>(CppToken::KW_ALIGNOF)},
        {"and", static_cast<TokenType>(CppToken::KW_AND)},
        {"and_eq", static_cast<TokenType>(CppToken::KW_AND_EQ)},
        {"asm", static_cast<TokenType>(CppToken::KW_ASM)},
        {"auto", static_cast<TokenType>(CppToken::KW_AUTO)},
        {"bitand", static_cast<TokenType>(CppToken::KW_BITAND)},
        {"bitor", static_cast<TokenType>(CppToken::KW_BITOR)},
        {"bool", static_cast<TokenType>(CppToken::KW_BOOL)},
        {"break", static_cast<TokenType>(CppToken::KW_BREAK)},
        {"case", static_cast<TokenType>(CppToken::KW_CASE)},
        {"catch", static_cast<TokenType>(CppToken::KW_CATCH)},
        {"char", static_cast<TokenType>(CppToken::KW_CHAR)},
        {"char8_t", static_cast<TokenType>(CppToken::KW_CHAR8_T)},
        {"char16_t", static_cast<TokenType>(CppToken::KW_CHAR16_T)},
        {"char32_t", static_cast<TokenType>(CppToken::KW_CHAR32_T)},
        {"class", static_cast<TokenType>(CppToken::KW_CLASS)},
        {"compl", static_cast<TokenType>(CppToken::KW_COMPL)},
        {"concept", static_cast<TokenType>(CppToken::KW_CONCEPT)},
        {"const", static_cast<TokenType>(CppToken::KW_CONST)},
        {"consteval", static_cast<TokenType>(CppToken::KW_CONSTEVAL)},
        {"constexpr", static_cast<TokenType>(CppToken::KW_CONSTEXPR)},
        {"constinit", static_cast<TokenType>(CppToken::KW_CONSTINIT)},
        {"const_cast", static_cast<TokenType>(CppToken::KW_CONST_CAST)},
        {"continue", static_cast<TokenType>(CppToken::KW_CONTINUE)},
        {"co_await", static_cast<TokenType>(CppToken::KW_CO_AWAIT)},
        {"co_return", static_cast<TokenType>(CppToken::KW_CO_RETURN)},
        {"co_yield", static_cast<TokenType>(CppToken::KW_CO_YIELD)},
        {"decltype", static_cast<TokenType>(CppToken::KW_DECLTYPE)},
        {"default", static_cast<TokenType>(CppToken::KW_DEFAULT)},
        {"delete", static_cast<TokenType>(CppToken::KW_DELETE)},
        {"do", static_cast<TokenType>(CppToken::KW_DO)},
        {"double", static_cast<TokenType>(CppToken::KW_DOUBLE)},
        {"dynamic_cast", static_cast<TokenType>(CppToken::KW_DYNAMIC_CAST)},
        {"else", static_cast<TokenType>(CppToken::KW_ELSE)},
        {"enum", static_cast<TokenType>(CppToken::KW_ENUM)},
        {"explicit", static_cast<TokenType>(CppToken::KW_EXPLICIT)},
        {"export", static_cast<TokenType>(CppToken::KW_EXPORT)},
        {"extern", static_cast<TokenType>(CppToken::KW_EXTERN)},
        {"false", static_cast<TokenType>(CppToken::KW_FALSE)},
        {"float", static_cast<TokenType>(CppToken::KW_FLOAT)},
        {"for", static_cast<TokenType>(CppToken::KW_FOR)},
        {"friend", static_cast<TokenType>(CppToken::KW_FRIEND)},
        {"goto", static_cast<TokenType>(CppToken::KW_GOTO)},
        {"if", static_cast<TokenType>(CppToken::KW_IF)},
        {"inline", static_cast<TokenType>(CppToken::KW_INLINE)},
        {"int", static_cast<TokenType>(CppToken::KW_INT)},
        {"long", static_cast<TokenType>(CppToken::KW_LONG)},
        {"mutable", static_cast<TokenType>(CppToken::KW_MUTABLE)},
        {"namespace", static_cast<TokenType>(CppToken::KW_NAMESPACE)},
        {"new", static_cast<TokenType>(CppToken::KW_NEW)},
        {"noexcept", static_cast<TokenType>(CppToken::KW_NOEXCEPT)},
        {"not", static_cast<TokenType>(CppToken::KW_NOT)},
        {"not_eq", static_cast<TokenType>(CppToken::KW_NOT_EQ)},
        {"nullptr", static_cast<TokenType>(CppToken::KW_NULLPTR)},
        {"operator", static_cast<TokenType>(CppToken::KW_OPERATOR)},
        {"or", static_cast<TokenType>(CppToken::KW_OR)},
        {"or_eq", static_cast<TokenType>(CppToken::KW_OR_EQ)},
        {"private", static_cast<TokenType>(CppToken::KW_PRIVATE)},
        {"protected", static_cast<TokenType>(CppToken::KW_PROTECTED)},
        {"public", static_cast<TokenType>(CppToken::KW_PUBLIC)},
        {"register", static_cast<TokenType>(CppToken::KW_REGISTER)},
        {"reinterpret_cast", static_cast<TokenType>(CppToken::KW_REINTERPRET_CAST)},
        {"requires", static_cast<TokenType>(CppToken::KW_REQUIRES)},
        {"return", static_cast<TokenType>(CppToken::KW_RETURN)},
        {"short", static_cast<TokenType>(CppToken::KW_SHORT)},
        {"signed", static_cast<TokenType>(CppToken::KW_SIGNED)},
        {"sizeof", static_cast<TokenType>(CppToken::KW_SIZEOF)},
        {"static", static_cast<TokenType>(CppToken::KW_STATIC)},
        {"static_assert", static_cast<TokenType>(CppToken::KW_STATIC_ASSERT)},
        {"static_cast", static_cast<TokenType>(CppToken::KW_STATIC_CAST)},
        {"struct", static_cast<TokenType>(CppToken::KW_STRUCT)},
        {"switch", static_cast<TokenType>(CppToken::KW_SWITCH)},
        {"synchronized", static_cast<TokenType>(CppToken::KW_SYNCHRONIZED)},
        {"template", static_cast<TokenType>(CppToken::KW_TEMPLATE)},
        {"this", static_cast<TokenType>(CppToken::KW_THIS)},
        {"thread_local", static_cast<TokenType>(CppToken::KW_THREAD_LOCAL)},
        {"throw", static_cast<TokenType>(CppToken::KW_THROW)},
        {"true", static_cast<TokenType>(CppToken::KW_TRUE)},
        {"try", static_cast<TokenType>(CppToken::KW_TRY)},
        {"typedef", static_cast<TokenType>(CppToken::KW_TYPEDEF)},
        {"typeid", static_cast<TokenType>(CppToken::KW_TYPEID)},
        {"typename", static_cast<TokenType>(CppToken::KW_TYPENAME)},
        {"union", static_cast<TokenType>(CppToken::KW_UNION)},
        {"unsigned", static_cast<TokenType>(CppToken::KW_UNSIGNED)},
        {"using", static_cast<TokenType>(CppToken::KW_USING)},
        {"virtual", static_cast<TokenType>(CppToken::KW_VIRTUAL)},
        {"void", static_cast<TokenType>(CppToken::KW_VOID)},
        {"volatile", static_cast<TokenType>(CppToken::KW_VOLATILE)},
        {"wchar_t", static_cast<TokenType>(CppToken::KW_WCHAR_T)},
        {"while", static_cast<TokenType>(CppToken::KW_WHILE)},
        {"xor", static_cast<TokenType>(CppToken::KW_XOR)},
        {"xor_eq", static_cast<TokenType>(CppToken::KW_XOR_EQ)}
    };
    return keywords;
}

Token Lexer::scanNumber() {
    ::Elica::Core::Position startPos(line, column);
    size_t startPosIdx = pos;

    bool isFloat = false;
    bool isHex = false;
    bool isBin = false;

    // 0x, 0X, 0b, 0B
    if (peek() == '0') {
        advance();
        if (peek() == 'x' || peek() == 'X') {
            advance();
            isHex = true;
            while (isxdigit(peek()) || peek() == '\'') {
                advance();
            }
        } else if (peek() == 'b' || peek() == 'B') {
            advance();
            isBin = true;
            while (peek() == '0' || peek() == '1' || peek() == '\'') {
                advance();
            }
        } else {
            while ((peek() >= '0'  && peek() <= '7') || peek() == '\'') {
                advance();
            }
        }
    }

    // standard
    if (!isHex  && !isBin) {
        while (isdigit(peek()) || peek() == '\'') {
            advance();
        }

        // .
        if (peek() == '.'  && (isdigit(source[pos + 1]) || source[pos + 1] == 'e' || source[pos + 1] == 'E')) {
            isFloat = true;
            advance(); // съедаем '.'
            while (isdigit(peek()) || peek() == '\'') {
                advance();
            }
        }

        // e
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance(); // съедаем 'e'/'E'
            if (peek() == '+' || peek() == '-') {
                advance(); // съедаем знак экспоненты
            }
            while (isdigit(peek()) || peek() == '\'') {
                advance();
            }
        }
    } else if (isHex) {
        if (peek() == '.') {
            isFloat = true;
            advance();
            while (isxdigit(peek()) || peek() == '\'') {
                advance();
            }
            if (peek() == 'p' || peek() == 'P') {
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (isdigit(peek())) advance();
            }
        }
    }

    // f, F, u, U, l, L, ll, LL, ULL, ull, ul etc.
    while (isalpha(peek())) {
        advance();
    }

    string lexeme = source.substr(startPosIdx, pos - startPosIdx);
    CommonToken type = isFloat ? CommonToken::FLT_LITERAL : CommonToken::INT_LITERAL;

    return Token(TokenVariant(type), lexeme, startPos);
}

Token Lexer::scanIdentifierOrKeyword() {
    ::Elica::Core::Position startPos(line, column);
    size_t startPosIdx = pos;

    while (isalnum(peek()) || peek() == '_') {
        advance();
    }

    string lexeme = source.substr(startPosIdx, pos - startPosIdx);

    const unordered_map<string, TokenType> &keywords = 
        (language == LexerLanguage::ELY) ? getElyKeywords() :
        (language == LexerLanguage::C) ? getCKeywords() : getCppKeywords();

    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        TokenType rawVal = it->second;
        TokenVariant variantVal;
        if (language == LexerLanguage::ELY) {
            variantVal = static_cast<ElyToken>(rawVal);
        } else if (language == LexerLanguage::C) {
            variantVal = static_cast<CToken>(rawVal);
        } else {
            variantVal = static_cast<CppToken>(rawVal);
        }
        return Token(variantVal, lexeme, startPos);
    }

    return Token(TokenVariant(CommonToken::IDENTIFIER), lexeme, startPos);
}

Token Lexer::scanString() {
    ::Elica::Core::Position startPos(line, column);
    
    bool isFString = false;
    if (peek() == 'f' || peek() == 'F') {
        isFString = true;
        advance();
    }

    bool isMultiline = false;
    if (peek() == '"'  && pos + 2 < source.size()  && source[pos + 1] == '"'  && source[pos + 2] == '"') {
        isMultiline = true;
        advance(); advance(); advance();
    } else {
        advance();
    }

    size_t contentStart = pos;
    string lexeme = "";

    if (isMultiline) {
        while (!isAtEnd()) {
            if (peek() == '"'  && pos + 2 < source.size()  && source[pos + 1] == '"'  && source[pos + 2] == '"') {
                break;
            }
            advance();
        }
        lexeme = source.substr(contentStart, pos - contentStart);
        advance(); advance(); advance();
    } else {
        while (!isAtEnd()  && peek() != '"') {
            if (peek() == '\\') {
                advance();
            }
            advance();
        }
        lexeme = source.substr(contentStart, pos - contentStart);
        if (!isAtEnd()) advance();
    }

    CommonToken tokenType;
    if (isFString  && isMultiline) tokenType = CommonToken::F_MULTILINE_STR_LITERAL;
    else if (isFString) tokenType = CommonToken::F_STR_LITERAL;
    else if (isMultiline) tokenType = CommonToken::MULTILINE_STR_LITERAL;
    else tokenType = CommonToken::STR_LITERAL;

    return Token(TokenVariant(tokenType), lexeme, startPos);
}

vector<Token> Lexer::tokenize() {
    vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) break;

        char c = peek();
        ::Elica::Core::Position currentPos(line, column);

        // str  &fstr
        if (c == '"' || ((c == 'f' || c == 'F')  && pos + 1 < source.size()  && source[pos + 1] == '"')) {
            tokens.push_back(scanString());
            continue;
        }

        // nums
        if (isdigit(c)) {
            tokens.push_back(scanNumber());
            continue;
        }

        // ids  &kws
        if (isalpha(c) || c == '_') {
            tokens.push_back(scanIdentifierOrKeyword());
            continue;
        }

        // symbols  &ops
        advance();
        switch (c) {
            case '+': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::PLUS_ASSIGN), "+=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::PLUS), "+", currentPos);
                break;
            case '-': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::MINUS_ASSIGN), "-=", currentPos);
                else if (match('>')) tokens.emplace_back(TokenVariant(CommonToken::ARROW), "->", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::MINUS), "-", currentPos);
                break;
            case '*': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::MUL_ASSIGN), "*=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::STAR), "*", currentPos);
                break;
            case '/': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::DIV_ASSIGN), "/=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::SLASH), "/", currentPos);
                break;
            case '%': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::MOD_ASSIGN), "%=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::PERCENT), "%", currentPos);
                break;
            case '=': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::EQUAL), "==", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::ASSIGN), "=", currentPos);
                break;
            case '!': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::NOT_EQUAL), "!=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::NOT), "!", currentPos);
                break;
            case '<': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::LESS_EQUAL), "<=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::LESS), "<", currentPos);
                break;
            case '>': 
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::GREATER_EQUAL), ">=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::GREATER), ">", currentPos);
                break;
            case '&': tokens.emplace_back(TokenVariant(CommonToken::AMPERSAND), "&", currentPos); break;
            case '#': tokens.emplace_back(TokenVariant(CommonToken::HASH), "#", currentPos); break;
            case '@': tokens.emplace_back(TokenVariant(CommonToken::AT), "@", currentPos); break;
            case '?': tokens.emplace_back(TokenVariant(CommonToken::QUESTION), "?", currentPos); break;
            case ':': tokens.emplace_back(TokenVariant(CommonToken::COLON), ":", currentPos); break;
            case ';': tokens.emplace_back(TokenVariant(CommonToken::SEMICOLON), ";", currentPos); break;
            case ',': tokens.emplace_back(TokenVariant(CommonToken::COMMA), ",", currentPos); break;
            case '.': tokens.emplace_back(TokenVariant(CommonToken::DOT), ".", currentPos); break;
            case '(': tokens.emplace_back(TokenVariant(CommonToken::LPAREN), "(", currentPos); break;
            case ')': tokens.emplace_back(TokenVariant(CommonToken::RPAREN), ")", currentPos); break;
            case '[': tokens.emplace_back(TokenVariant(CommonToken::LBRACKET), "[", currentPos); break;
            case ']': tokens.emplace_back(TokenVariant(CommonToken::RBRACKET), "]", currentPos); break;
            case '{': tokens.emplace_back(TokenVariant(CommonToken::LBRACE), "{", currentPos); break;
            case '}': tokens.emplace_back(TokenVariant(CommonToken::RBRACE), "}", currentPos); break;
            default:
                cerr << "[Elica][LEXER ERROR] Unexpected character '" << c << "' at line " << currentPos.line << "\n";
                break;
        }
    }

    tokens.emplace_back(TokenVariant(CommonToken::END_OF_FILE), "", ::Elica::Core::Position(line, column));
    return tokens;
}

} // namespace Elica::Lexer