#pragma once
#include <string>
#include <vector>
#include "Tokens.h"

class Lexer
{
public:
    Lexer() = default;



    std::vector<Token> tokenize(const std::string& source);

private:
    std::string m_source;
    size_t      m_pos;
    int         m_line;

    char current() const;
    char peek(int offset = 1) const;
    void advance();
    void skipWhitespace();

    Token readNumber();
    Token readIdentifierOrKeyword();
    Token readRegister();
    Token readDirective();
    Token readString();
    Token readComment();
};
