#include "Emulator.h"

static const char* tokenTypeName(TokenType t)
{
    switch (t) {
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::NUMBER:      return "NUMBER";
        case TokenType::REGISTER:    return "REGISTER";
        case TokenType::DIRECTIVE:   return "DIRECTIVE";
        case TokenType::COLON:       return "COLON";
        case TokenType::COMMA:       return "COMMA";
        case TokenType::LPAREN:      return "LPAREN";
        case TokenType::RPAREN:      return "RPAREN";
        case TokenType::STRING:      return "STRING";
        case TokenType::COMMENT:     return "COMMENT";
        case TokenType::NEWLINE:     return "NEWLINE";
        case TokenType::END_OF_FILE: return "EOF";
        default:                     return "UNKNOWN";
    }
}

void Emulator::debugTokens(const std::vector<Token>& tokens)
{
    m_buffer.clear();
    m_buffer.m_data += "=== Lexer ===\n";
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::NEWLINE || tok.type == TokenType::END_OF_FILE) {
            m_buffer.m_data += '\n';
            continue;
        }
        m_buffer.m_data += '[' + std::string(tokenTypeName(tok.type)) + ' ' + tok.value + ']' + " (line " + std::to_string(tok.line) + ")\n";
    }
}

void Emulator::debugIR(const Program& program)
{
    m_buffer.clear();
    m_buffer.m_data += "=== Parser ===\n";
    for (const auto& node : program) {
        std::visit([this](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, Label>) {
                m_buffer.m_data += "[LABEL]       " + s.name + '\n';
            } else if constexpr (std::is_same_v<T, Directive>) {
                m_buffer.m_data += "[DIRECTIVE]   " + s.name;
                for (const auto& arg : s.args)
                    m_buffer.m_data += ' ' + arg.value;
                m_buffer.m_data += '\n';
            } else if constexpr (std::is_same_v<T, Instruction>) {
                m_buffer.m_data += "[INSTRUCTION] " + s.opcode;
                for (const auto& op : s.operands)
                    m_buffer.m_data += ' ' + op.value;
                m_buffer.m_data += '\n';
            }
        }, node);
    }
}


void Emulator::halt()
{
    // Marks the CPU halted so the run()/step() loop currently in progress
    // stops at the next opportunity. Safe to call from inside a syscall
    // callback (e.g. an input dialog being cancelled): the in-flight
    // instruction finishes normally, then run()'s while(!halted()) check
    // exits on its next iteration.
    m_cpu.m_halted = true;
}

void Emulator::reset()
{
    m_cpu.m_halted = false;
    m_cpu.m_breakpointHit = false;
    m_cpu.pc = m_cpu.text;
    
    // --- ADD THESE LINES TO CLEAR THE PIPELINE ---
    m_globalCycle = 0;
    m_pipelineHistory.clear();
    for (int i = 0; i < 5; ++i) {
        m_activeInsts[i] = -1;
    }
    
    m_cpu = MipsCPU{};   // resets pc, registers, halted, decoded fields
    m_memory = Memory{};
    m_buffer.clear();
    m_errors.clear();
}