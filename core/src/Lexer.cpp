#include "Lexer.h"
#include <stdexcept>
#include <cctype>



char Lexer::current() const
{
    return m_pos < m_source.size() ? m_source[m_pos] : '\0';
}

char Lexer::peek(int offset) const
{
    size_t idx = m_pos + offset;
    return idx < m_source.size() ? m_source[idx] : '\0';
}

void Lexer::advance()
{
    if (m_pos < m_source.size()) {
        if (m_source[m_pos] == '\n') ++m_line;
        ++m_pos;
    }
}

void Lexer::skipWhitespace()
{
    while (m_pos < m_source.size() &&
           current() != '\n' &&
           std::isspace(static_cast<unsigned char>(current())))
        advance();
}

Token Lexer::readNumber()
{
    std::string val;
    int startLine = m_line;

    // consume optional leading sign
    if (current() == '-' || current() == '+') {
        val += current(); advance();
    }

    // hex: 0x...
    if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
        val += current(); advance();
        val += current(); advance();
        while (std::isxdigit(static_cast<unsigned char>(current()))) {
            val += current(); advance();
        }
    } else {
        while (std::isdigit(static_cast<unsigned char>(current()))) {
            val += current(); advance();
        }
    }
    return Token(TokenType::NUMBER, val, startLine);
}

Token Lexer::readIdentifierOrKeyword()
{
    std::string val;
    int startLine = m_line;
    while (std::isalnum(static_cast<unsigned char>(current())) || current() == '_') {
        val += current(); advance();
    }
    return Token(TokenType::IDENTIFIER, val, startLine);
}

Token Lexer::readRegister()
{
    // '$' already consumed by caller; read the register name
    std::string val = "$";
    int startLine = m_line;
    while (std::isalnum(static_cast<unsigned char>(current())) || current() == '_') {
        val += current(); advance();
    }
    return Token(TokenType::REGISTER, val, startLine);
}

Token Lexer::readDirective()
{
    // '.' already consumed by caller
    std::string val = ".";
    int startLine = m_line;
    while (std::isalpha(static_cast<unsigned char>(current()))) {
        val += current(); advance();
    }
    return Token(TokenType::DIRECTIVE, val, startLine);
}

Token Lexer::readString()
{
    // opening '"' already consumed by caller
    std::string val;
    int startLine = m_line;
    while (m_pos < m_source.size() && current() != '"') {
        if (current() == '\\') {
            advance(); // consume backslash
            switch (current()) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case 'r':  val += '\r'; break;
                case '0':  val += '\0'; break;
                case '\\': val += '\\'; break;
                case '"':  val += '"';  break;
                default:   val += current(); break;
            }
        } else {
            val += current();
        }
        advance();
    }
    if (current() == '"') advance(); // consume closing quote
    return Token(TokenType::STRING, val, startLine);
}

Token Lexer::readComment()
{
    // '#' already consumed by caller
    std::string val;
    int startLine = m_line;
    while (m_pos < m_source.size() && current() != '\n') {
        val += current(); advance();
    }
    return Token(TokenType::COMMENT, val, startLine);
}

std::vector<Token> Lexer::tokenize(const std::string& source)
{
    m_source = source;
    m_pos    = 0;
    m_line   = 1;

    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();

        if (m_pos >= m_source.size()) {
            tokens.emplace_back(TokenType::END_OF_FILE, "", m_line);
            break;
        }

        char c = current();
        int  ln = m_line;

        if (c == '\n') {
            advance();
            // collapse consecutive blank lines into one NEWLINE
            while (current() == '\n') advance();
            tokens.emplace_back(TokenType::NEWLINE, "\n", ln);
        } else if (c == '#') {
            advance();
            tokens.push_back(readComment());
        } else if (c == '"') {
            advance();
            tokens.push_back(readString());
        } else if (c == '$') {
            advance();
            tokens.push_back(readRegister());
        } else if (c == '.') {
            advance();
            tokens.push_back(readDirective());
        } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                   ((c == '-' || c == '+') &&
                    std::isdigit(static_cast<unsigned char>(peek())))) {
            tokens.push_back(readNumber());
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
        } else {
            advance();
            switch (c) {
                case ':': tokens.emplace_back(TokenType::COLON,  ":", ln); break;
                case ',': tokens.emplace_back(TokenType::COMMA,  ",", ln); break;
                case '(': tokens.emplace_back(TokenType::LPAREN, "(", ln); break;
                case ')': tokens.emplace_back(TokenType::RPAREN, ")", ln); break;
                default:
                    throw std::runtime_error(
                        std::string("Unexpected character '") + c +
                        "' at line " + std::to_string(ln));
            }
        }
    }

    return tokens;
}
