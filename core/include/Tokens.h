
#pragma once
#include <string>

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    REGISTER,
    DIRECTIVE,
    COLON,
    COMMA,
    LPAREN,
    RPAREN,
    STRING,
    COMMENT,
    NEWLINE,
    END_OF_FILE
};


class Token
{
public:
    Token(TokenType type, std::string value, int line)
        : type(type), value(std::move(value)), line(line) {}

    TokenType   type;
    std::string value;
    int         line;
};