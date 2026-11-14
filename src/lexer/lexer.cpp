// SPDX-License-Identifier: MIT
#include "lexer/lexer.h"
#include <cctype>
#include <iostream>

using namespace std;
namespace fs = ::std::filesystem;

namespace Elica::Lexer {

std::string tokenTypeName(const Token &token) {
    if (std::holds_alternative<CommonToken>(token.type)) {
        switch (std::get<CommonToken>(token.type)) {
#define C(V) case CommonToken::V: return #V;
            C(END_OF_FILE) C(IDENTIFIER) C(INT_LITERAL) C(FLT_LITERAL)
            C(STR_LITERAL) C(F_STR_LITERAL) C(MULTILINE_STR_LITERAL)
            C(F_MULTILINE_STR_LITERAL) C(CHAR_LITERAL)
            C(PLUS) C(MINUS) C(STAR) C(SLASH) C(PERCENT) C(ASSIGN)
            C(NOT) C(LESS) C(GREATER) C(AMPERSAND) C(HASH) C(AT)
            C(QUESTION) C(COLON) C(SEMICOLON) C(COMMA) C(DOT)
            C(EQUAL) C(NOT_EQUAL) C(LESS_EQUAL) C(GREATER_EQUAL)
            C(PLUS_ASSIGN) C(MINUS_ASSIGN) C(MUL_ASSIGN) C(DIV_ASSIGN)
            C(MOD_ASSIGN) C(ARROW) C(LPAREN) C(RPAREN) C(LBRACKET)
            C(RBRACKET) C(LBRACE) C(RBRACE)
            C(PLUS_PLUS) C(MINUS_MINUS) C(CARET) C(TILDE) C(PIPE)
            C(SHL) C(SHR) C(LOGICAL_AND) C(LOGICAL_OR) C(HASH_HASH)
            C(SCOPE) C(ELLIPSIS) C(DOT_STAR) C(ARROW_STAR)
            C(SHL_ASSIGN) C(SHR_ASSIGN) C(AND_ASSIGN) C(OR_ASSIGN)
            C(XOR_ASSIGN) C(NATIVE_CODE)
#undef C
            default: return "COMMON?";
        }
    }
    if (std::holds_alternative<ElyToken>(token.type)) {
        switch (std::get<ElyToken>(token.type)) {
#define E(V) case ElyToken::V: return "ELY:" #V;
            E(KW_BYTE) E(KW_UBYTE) E(KW_INT) E(KW_UINT) E(KW_SHORT)
            E(KW_USHORT) E(KW_MORE) E(KW_UMORE) E(KW_FLT) E(KW_DOUBLE)
            E(KW_NOISED) E(KW_STR) E(KW_ATOM) E(KW_CHAR) E(KW_BOOL)
            E(KW_VOID) E(KW_SENTINEL) E(KW_ANY) E(KW_FUNC) E(KW_CLASS)
            E(KW_STRUCT) E(KW_cCode) E(KW_cppCode) E(KW_PUBLIC)
            E(KW_PRIVATE) E(KW_MINE) E(KW_STATIC) E(KW_COPA) E(KW_CONST)
            E(KW_NOCOPA) E(KW_SEALED) E(KW_ABSTRACT) E(KW_WAIT)
            E(KW_UNWAIT) E(KW_STACK) E(KW_HEAP) E(KW_IF) E(KW_ELSE)
            E(KW_WHILE) E(KW_FOR) E(KW_RETURN) E(KW_TRY) E(KW_RAISE)
            E(KW_EXCEPT) E(KW_BREAK) E(KW_CONTINUE) E(KW_USING)
            E(KW_TRUE) E(KW_FALSE) E(KW_IS) E(KW_IN) E(KW_LAMBDA)
            E(KW_NOT) E(KW_THIS)
#undef E
            default: return "ELY?";
        }
    }
    if (std::holds_alternative<CToken>(token.type)) {
        switch (std::get<CToken>(token.type)) {
#define CC(V) case CToken::V: return "C:" #V;
            CC(KW_AUTO) CC(KW_BREAK) CC(KW_CASE) CC(KW_CHAR) CC(KW_CONST)
            CC(KW_CONTINUE) CC(KW_DEFAULT) CC(KW_DO) CC(KW_DOUBLE)
            CC(KW_ELSE) CC(KW_ENUM) CC(KW_EXTERN) CC(KW_FLOAT) CC(KW_FOR)
            CC(KW_GOTO) CC(KW_IF) CC(KW_INLINE) CC(KW_INT) CC(KW_LONG)
            CC(KW_REGISTER) CC(KW_RESTRICT) CC(KW_RETURN) CC(KW_SHORT)
            CC(KW_SIGNED) CC(KW_SIZEOF) CC(KW_STATIC) CC(KW_STRUCT)
            CC(KW_SWITCH) CC(KW_TYPEDEF) CC(KW_UNION) CC(KW_UNSIGNED)
            CC(KW_VOID) CC(KW_VOLATILE) CC(KW_WHILE) CC(KW_ALIGNAS)
            CC(KW_ALIGNOF) CC(KW_ATOMIC) CC(KW_BOOL) CC(KW_COMPLEX)
            CC(KW_GENERIC) CC(KW_IMAGINARY) CC(KW_NORETURN)
            CC(KW_STATIC_ASSERT) CC(KW_THREAD_LOCAL) CC(KW_FALSE)
            CC(KW_TRUE) CC(KW_CONSTEXPR) CC(KW_NULLPTR) CC(KW_TYPEOF)
            CC(KW_TYPEOF_UNQUAL)
#undef CC
            default: return "C?";
        }
    }
    if (std::holds_alternative<CppToken>(token.type)) {
        switch (std::get<CppToken>(token.type)) {
#define CP(V) case CppToken::V: return "CPP:" #V;
            CP(KW_ALIGNAS) CP(KW_ALIGNOF) CP(KW_AND) CP(KW_AND_EQ)
            CP(KW_ASM) CP(KW_AUTO) CP(KW_BITAND) CP(KW_BITOR) CP(KW_BOOL)
            CP(KW_BREAK) CP(KW_CASE) CP(KW_CATCH) CP(KW_CHAR)
            CP(KW_CHAR8_T) CP(KW_CHAR16_T) CP(KW_CHAR32_T) CP(KW_CLASS)
            CP(KW_COMPL) CP(KW_CONCEPT) CP(KW_CONST) CP(KW_CONSTEVAL)
            CP(KW_CONSTEXPR) CP(KW_CONSTINIT) CP(KW_CONST_CAST)
            CP(KW_CONTINUE) CP(KW_CO_AWAIT) CP(KW_CO_RETURN)
            CP(KW_CO_YIELD) CP(KW_DECLTYPE) CP(KW_DEFAULT) CP(KW_DELETE)
            CP(KW_DO) CP(KW_DOUBLE) CP(KW_DYNAMIC_CAST) CP(KW_ELSE)
            CP(KW_ENUM) CP(KW_EXPLICIT) CP(KW_EXPORT) CP(KW_EXTERN)
            CP(KW_FALSE) CP(KW_FLOAT) CP(KW_FOR) CP(KW_FRIEND)
            CP(KW_GOTO) CP(KW_IF) CP(KW_INLINE) CP(KW_INT) CP(KW_LONG)
            CP(KW_MUTABLE) CP(KW_NAMESPACE) CP(KW_NEW) CP(KW_NOEXCEPT)
            CP(KW_NOT) CP(KW_NOT_EQ) CP(KW_NULLPTR) CP(KW_OPERATOR)
            CP(KW_OR) CP(KW_OR_EQ) CP(KW_PRIVATE) CP(KW_PROTECTED)
            CP(KW_PUBLIC) CP(KW_REGISTER) CP(KW_REINTERPRET_CAST)
            CP(KW_REQUIRES) CP(KW_RETURN) CP(KW_SHORT) CP(KW_SIGNED)
            CP(KW_SIZEOF) CP(KW_STATIC) CP(KW_STATIC_ASSERT)
            CP(KW_STATIC_CAST) CP(KW_STRUCT) CP(KW_SWITCH)
            CP(KW_SYNCHRONIZED) CP(KW_TEMPLATE) CP(KW_THIS)
            CP(KW_THREAD_LOCAL) CP(KW_THROW) CP(KW_TRUE) CP(KW_TRY)
            CP(KW_TYPEDEF) CP(KW_TYPEID) CP(KW_TYPENAME) CP(KW_UNION)
            CP(KW_UNSIGNED) CP(KW_USING) CP(KW_VIRTUAL) CP(KW_VOID)
            CP(KW_VOLATILE) CP(KW_WCHAR_T) CP(KW_WHILE) CP(KW_XOR)
            CP(KW_XOR_EQ) CP(KW_ATOMIC_CANCEL) CP(KW_ATOMIC_COMMIT)
            CP(KW_ATOMIC_NOEXCEPT)
#undef CP
            default: return "CPP?";
        }
    }
    return "?";
}

Lexer::Lexer(string src, LexerLanguage lang)
    : source(move(src)), language(lang) {}

char Lexer::peek() const {
    if (pos >= source.size()) return '\0';
    return source[pos];
}

char Lexer::peekNext() const {
    if (pos + 1 >= source.size()) return '\0';
    return source[pos + 1];
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
        {"true", static_cast<TokenType>(CToken::KW_TRUE)},
        // C23
        {"constexpr", static_cast<TokenType>(CToken::KW_CONSTEXPR)},
        {"nullptr", static_cast<TokenType>(CToken::KW_NULLPTR)},
        {"typeof", static_cast<TokenType>(CToken::KW_TYPEOF)},
        {"typeof_unqual", static_cast<TokenType>(CToken::KW_TYPEOF_UNQUAL)}
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
        {"xor_eq", static_cast<TokenType>(CppToken::KW_XOR_EQ)},
        // C++26
        {"atomic_cancel", static_cast<TokenType>(CppToken::KW_ATOMIC_CANCEL)},
        {"atomic_commit", static_cast<TokenType>(CppToken::KW_ATOMIC_COMMIT)},
        {"atomic_noexcept", static_cast<TokenType>(CppToken::KW_ATOMIC_NOEXCEPT)}
    };
    return keywords;
}

Token Lexer::scanNumber() {
    ::Elica::Core::Position startPos(line, column);
    size_t startPosIdx = pos;

    bool isFloat = false;
    bool isHex = false;
    bool isBin = false;

    // leading-dot float: .5  (tokenize() routes '.'+digit here)
    if (peek() == '.') {
        isFloat = true;
        advance();
    }

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
        }
        // hex float exponent: 0x1p3 / 0xA.Bp-2 (C99 / C++17)
        if (peek() == 'p' || peek() == 'P') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (isdigit(peek()) || peek() == '\'') advance();
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

    // 0) C++ raw string literal: R"delim( ... )delim"  (max 16-char delim)
    if (language != LexerLanguage::ELY && peek() == 'R'
        && pos + 1 < source.size() && source[pos + 1] == '"') {
        advance();                          // R
        advance();                          // "
        string delim;
        while (!isAtEnd() && peek() != '('
               && peek() != '\\' && peek() != ')' && peek() != ' '
               && peek() != '\t' && peek() != '\n' && delim.size() < 16) {
            delim += advance();
        }
        if (!isAtEnd() && peek() == '(') advance();
        string lexeme;
        while (!isAtEnd()) {
            const bool close =
                (peek() == ')' && pos + 1 + delim.size() < source.size()
                 && source.compare(pos + 1, delim.size(), delim) == 0
                 && source[pos + 1 + delim.size()] == '"');
            if (close) break;
            lexeme += advance();
        }
        if (!isAtEnd()) advance();                       // )
        for (size_t i = 0; i < delim.size() && !isAtEnd(); ++i) advance();
        if (!isAtEnd()) advance();                       // "
        return Token(TokenVariant(CommonToken::STR_LITERAL), lexeme, startPos);
    }

    // 1) C/C++ prefixes: L" / u" / U" / u8"  and  L' / u' / U' / u8'
    if (language != LexerLanguage::ELY
        && (peek() == 'L' || peek() == 'u' || peek() == 'U')) {
        const char p = peek();
        const char n1 = peekNext();
        if (n1 == '"' || n1 == '\'') {
            advance();                       // single-letter prefix
        } else if (p == 'u' && n1 == '8'
                   && pos + 2 < source.size()
                   && (source[pos + 2] == '"' || source[pos + 2] == '\'')) {
            advance();                       // u
            advance();                       // 8
        }
    }

    // 2) character literal: ' ... ' (escape sequences included)
    if (peek() == '\'') {
        advance();
        string lexeme;
        while (!isAtEnd() && peek() != '\'') {
            if (peek() == '\\') {            // escape: \n, \x41, \' ...
                lexeme += advance();
                if (!isAtEnd()) lexeme += advance();
            } else {
                lexeme += advance();
            }
        }
        if (!isAtEnd()) advance();           // closing '
        return Token(TokenVariant(CommonToken::CHAR_LITERAL), lexeme, startPos);
    }

    bool isFString = false;
    if (peek() == 'f' || peek() == 'F') {
        isFString = true;
        advance();
    }

    bool isMultiline = false;
    if (language == LexerLanguage::ELY
        && peek() == '"'  && pos + 2 < source.size()
        && source[pos + 1] == '"' && source[pos + 2] == '"') {
        isMultiline = true;
        advance(); advance(); advance();
    } else {
        advance();                           // opening "
    }

    size_t contentStart = pos;
    string lexeme = "";

    if (isMultiline) {
        while (!isAtEnd()) {
            if (peek() == '"'  && pos + 2 < source.size()
                && source[pos + 1] == '"'  && source[pos + 2] == '"') {
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

bool Lexer::scanCodeBlock(LexerLanguage blockLang, std::vector<Token> &out) {
    // The `cCode`/`cppCode` keyword token was already emitted. The cursor is
    // somewhere before the opening '{' (whitespace allowed).
    while (!isAtEnd()  && isspace(static_cast<unsigned char>(peek()))) advance();
    if (isAtEnd() || peek() != '{') return false;

    const ::Elica::Core::Position bracePos(line, column);
    advance();                                    // {

    const size_t contentStart = pos;
    int depth = 1;
    size_t contentEnd = source.size();
    ::Elica::Core::Position closePos(0, 0);

    // Skip one quoted literal ('...' or "..."), escapes included.
    auto skipQuoted = [&](char q) {
        advance();                                // opening quote
        while (!isAtEnd()  && peek() != q) {
            if (peek() == '\\') advance();
            if (!isAtEnd()) advance();
        }
        if (!isAtEnd()) advance();                // closing quote
    };

    while (!isAtEnd()  && depth > 0) {
        const char ch = peek();

        if (ch == '"' || ch == '\'') { skipQuoted(ch); continue; }

        if (ch == '/'  && peekNext() == '/') {    // line comment
            while (!isAtEnd()  && peek() != '\n') advance();
            continue;
        }
        if (ch == '/'  && peekNext() == '*') {    // block comment
            advance(); advance();
            while (!isAtEnd()  && !(peek() == '*'  && peekNext() == '/')) advance();
            if (!isAtEnd()) { advance(); advance(); }
            continue;
        }
        if (blockLang == LexerLanguage::CPP  && ch == 'R'  && peekNext() == '"') {
            // raw string R"delim( ... )delim" — braces inside must not count
            advance(); advance();                 // R "
            string delim;
            while (!isAtEnd()  && peek() != '('  && delim.size() < 16
                   && peek() != '\\'  && peek() != ')'  && peek() != ' '
                   && peek() != '\t'  && peek() != '\n') {
                delim += advance();
            }
            if (!isAtEnd()  && peek() == '(') {
                advance();
                while (!isAtEnd()) {
                    if (peek() == ')'  && pos + 1 + delim.size() < source.size()
                        && source.compare(pos + 1, delim.size(), delim) == 0
                        && source[pos + 1 + delim.size()] == '"') {
                        for (size_t i = 0; i < delim.size() + 2  && !isAtEnd(); ++i) advance();
                        break;
                    }
                    advance();
                }
            }
            continue;
        }

        if (ch == '{') depth++;
        else if (ch == '}') {
            depth--;
            if (depth == 0) {
                closePos   = ::Elica::Core::Position(line, column);
                contentEnd = pos;
                advance();                        // }
                break;
            }
        }
        advance();
    }

    if (depth != 0) {
        cerr << "[Elica][LEXER ERROR] Unbalanced cCode/cppCode block at "
             << bracePos.line << ":" << bracePos.column << "\n";
        return true;
    }

    out.emplace_back(TokenVariant(CommonToken::LBRACE), "{", bracePos);

    // Lex the block body with the C/C++ lexer; shift positions to global ones.
    const string content = source.substr(contentStart, contentEnd - contentStart);
    Lexer subLexer(content, blockLang);
    for (auto &st : subLexer.tokenize()) {
        if (std::holds_alternative<CommonToken>(st.type)
            && std::get<CommonToken>(st.type) == CommonToken::END_OF_FILE) continue;
        st.position.line += bracePos.line - 1;
        if (st.position.line == bracePos.line) {
            st.position.column += bracePos.column;
        }
        out.push_back(st);
    }

    out.emplace_back(TokenVariant(CommonToken::RBRACE), "}", closePos);
    return true;
}

vector<Token> Lexer::tokenize() {
    vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) break;

        char c = peek();
        ::Elica::Core::Position currentPos(line, column);

        // strings: "..." ; char literals '...' ; raw/wide-prefixed forms
        if (c == '"' || c == '\'') {
            tokens.push_back(scanString());
            continue;
        }

        // Ely f-strings: f"..." / f"""..."""
        if (language == LexerLanguage::ELY
            && (c == 'f' || c == 'F')  && pos + 1 < source.size()  && source[pos + 1] == '"') {
            tokens.push_back(scanString());
            continue;
        }

        // C/C++ prefixes: L" u" U" u8" / L' u' U' u8'
        if (language != LexerLanguage::ELY
            && (c == 'L' || c == 'u' || c == 'U')) {
            const char n1 = peekNext();
            if (n1 == '"' || n1 == '\''
                || (c == 'u' && n1 == '8' && pos + 2 < source.size()
                    && (source[pos + 2] == '"' || source[pos + 2] == '\''))) {
                tokens.push_back(scanString());
                continue;
            }
        }

        // C++ raw strings: R"( ... )" / R"delim( ... )delim"
        if (language != LexerLanguage::ELY
            && c == 'R' && pos + 1 < source.size() && source[pos + 1] == '"') {
            tokens.push_back(scanString());
            continue;
        }

        // nums
        if (isdigit(c) || (c == '.' && isdigit(peekNext()))) {
            tokens.push_back(scanNumber());
            continue;
        }

        // ids  &kws
        if (isalpha(c) || c == '_') {
            Token idTok = scanIdentifierOrKeyword();
            tokens.push_back(idTok);

            // cCode { } / cppCode { } — lex the block body in C/C++ mode
            if (language == LexerLanguage::ELY
                && std::holds_alternative<ElyToken>(idTok.type)) {
                const auto et = std::get<ElyToken>(idTok.type);
                LexerLanguage blockLang;
                bool isCodeBlock = true;
                if (et == ElyToken::KW_cppCode) blockLang = LexerLanguage::CPP;
                else if (et == ElyToken::KW_cCode) blockLang = LexerLanguage::C;
                else isCodeBlock = false;
                if (isCodeBlock) scanCodeBlock(blockLang, tokens);
            }
            continue;
        }

        // symbols  &ops
        advance();
        switch (c) {
            case '+':
                if (match('+')) tokens.emplace_back(TokenVariant(CommonToken::PLUS_PLUS), "++", currentPos);
                else if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::PLUS_ASSIGN), "+=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::PLUS), "+", currentPos);
                break;
            case '-':
                if (match('-')) tokens.emplace_back(TokenVariant(CommonToken::MINUS_MINUS), "--", currentPos);
                else if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::MINUS_ASSIGN), "-=", currentPos);
                else if (match('>')) {
                    if (match('*')) tokens.emplace_back(TokenVariant(CommonToken::ARROW_STAR), "->*", currentPos);
                    else tokens.emplace_back(TokenVariant(CommonToken::ARROW), "->", currentPos);
                }
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
            case '^':
                if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::XOR_ASSIGN), "^=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::CARET), "^", currentPos);
                break;
            case '~': tokens.emplace_back(TokenVariant(CommonToken::TILDE), "~", currentPos); break;
            case '|':
                if (match('|')) tokens.emplace_back(TokenVariant(CommonToken::LOGICAL_OR), "||", currentPos);
                else if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::OR_ASSIGN), "|=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::PIPE), "|", currentPos);
                break;
            case '&':
                if (match('&')) tokens.emplace_back(TokenVariant(CommonToken::LOGICAL_AND), "&&", currentPos);
                else if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::AND_ASSIGN), "&=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::AMPERSAND), "&", currentPos);
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
                if (match('<')) {
                    if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::SHL_ASSIGN), "<<=", currentPos);
                    else tokens.emplace_back(TokenVariant(CommonToken::SHL), "<<", currentPos);
                }
                else if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::LESS_EQUAL), "<=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::LESS), "<", currentPos);
                break;
            case '>':
                if (match('>')) {
                    if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::SHR_ASSIGN), ">>=", currentPos);
                    else tokens.emplace_back(TokenVariant(CommonToken::SHR), ">>", currentPos);
                }
                else if (match('=')) tokens.emplace_back(TokenVariant(CommonToken::GREATER_EQUAL), ">=", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::GREATER), ">", currentPos);
                break;
            case '#':
                if (match('#')) tokens.emplace_back(TokenVariant(CommonToken::HASH_HASH), "##", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::HASH), "#", currentPos);
                break;
            case '@': tokens.emplace_back(TokenVariant(CommonToken::AT), "@", currentPos); break;
            case '?': tokens.emplace_back(TokenVariant(CommonToken::QUESTION), "?", currentPos); break;
            case ':':
                if (match(':')) tokens.emplace_back(TokenVariant(CommonToken::SCOPE), "::", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::COLON), ":", currentPos);
                break;
            case ';': tokens.emplace_back(TokenVariant(CommonToken::SEMICOLON), ";", currentPos); break;
            case ',': tokens.emplace_back(TokenVariant(CommonToken::COMMA), ",", currentPos); break;
            case '.':
                if (match('.') && match('.')) tokens.emplace_back(TokenVariant(CommonToken::ELLIPSIS), "...", currentPos);
                else if (match('*')) tokens.emplace_back(TokenVariant(CommonToken::DOT_STAR), ".*", currentPos);
                else tokens.emplace_back(TokenVariant(CommonToken::DOT), ".", currentPos);
                break;
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