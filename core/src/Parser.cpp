#include "Parser.h"

// ─── Cursor ──────────────────────────────────────────────────────────────────

struct Cursor {
    const std::vector<Token>& tokens;
    size_t pos = 0;

    Cursor(const std::vector<Token>& t) : tokens(t) {}

    const Token& current() const { return tokens[pos]; }

    const Token& peek(int offset = 1) const {
        size_t idx = pos + offset;
        return idx < tokens.size() ? tokens[idx] : tokens.back();
    }

    bool at_end() const { return tokens[pos].type == TokenType::END_OF_FILE; }

    const Token& advance() { return tokens[pos++]; }

    bool match(TokenType type) {
        if (current().type == type) { ++pos; return true; }
        return false;
    }

    const Token& expect(TokenType type) {
        if (current().type != type)
            throw std::runtime_error(
                "unexpected token '" + current().value +
                "' at line " + std::to_string(current().line));
        return advance();
    }

    bool isEndOfLine() const {
        auto t = current().type;
        return t == TokenType::NEWLINE ||
               t == TokenType::END_OF_FILE ||
               t == TokenType::COMMENT;
    }

    void skipToNextLine() {
        while (!at_end() && current().type != TokenType::NEWLINE)
            advance();
        match(TokenType::NEWLINE);
    }
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void skipLineEnd(Cursor& c)
{
    if (c.current().type == TokenType::COMMENT)
        c.skipToNextLine();
    else
        c.match(TokenType::NEWLINE);
}

// ─── Parse functions ─────────────────────────────────────────────────────────

// IDENTIFIER COLON  — emitted as a standalone Label node.
// Does NOT consume the newline: there may be an instruction on the same line.
static void parseLabel(Cursor& c, Program& program)
{
    Label lbl;
    lbl.name = c.current().value;
    lbl.line = c.current().line;
    c.advance();             // consume IDENTIFIER
    c.expect(TokenType::COLON); // consume COLON
    program.push_back(lbl);
}

// DIRECTIVE [args...] NEWLINE
static void parseDirective(Cursor& c, Program& program)
{
    Directive dir;
    dir.name = c.current().value;
    dir.line = c.current().line;
    c.advance(); // consume DIRECTIVE token

    while (!c.isEndOfLine()) {
        if (c.match(TokenType::COMMA)) continue; // skip separators
        dir.args.push_back(c.advance());
    }

    skipLineEnd(c);
    program.push_back(dir);
}

// IDENTIFIER [operands...] NEWLINE
static void parseInstruction(Cursor& c, Program& program)
{
    Instruction instr;
    instr.opcode = c.current().value;
    instr.line   = c.current().line;
    c.advance(); // consume mnemonic

    while (!c.isEndOfLine()) {
        if (c.match(TokenType::COMMA)) continue; // skip separators

        // Offset addressing: NUMBER ( REGISTER ) — store offset and base, drop parens
        if (c.current().type == TokenType::NUMBER &&
            c.peek().type    == TokenType::LPAREN)
        {
            instr.operands.push_back(c.advance()); // NUMBER  (offset)
            c.advance();                            // discard LPAREN
            instr.operands.push_back(c.advance()); // REGISTER (base)
            c.expect(TokenType::RPAREN);            // consume and validate RPAREN
            continue;
        }

        instr.operands.push_back(c.advance());
    }

    skipLineEnd(c);
    program.push_back(instr);
}

// ─── Parser::parse ───────────────────────────────────────────────────────────

auto Parser::parse(const std::vector<Token>& tokens) -> Program
{
    Program program;
    Cursor c(tokens);

    while (!c.at_end()) {
        switch (c.current().type) {
            case TokenType::NEWLINE:
                c.advance();
                break;

            case TokenType::COMMENT:
                c.skipToNextLine();
                break;

            case TokenType::DIRECTIVE:
                parseDirective(c, program);
                break;

            case TokenType::IDENTIFIER:
                if (c.peek().type == TokenType::COLON)
                    parseLabel(c, program);
                else
                    parseInstruction(c, program);
                break;

            default:
                throw std::runtime_error(
                    "unexpected token '" + c.current().value +
                    "' at line " + std::to_string(c.current().line));
        }
    }

    return program;
}
