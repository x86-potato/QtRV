#pragma once

#include <string>
#include <vector>

#include "Lexer.h"
#include "Assembler.h"  
#include "MipsCPU.h"
#include "Buffer.h"
#include "Binary.h"
#include "parser.h"


class Emulator
{
public:
    Emulator() = default;

    template <typename Callback>
    bool loadProgram(const std::string& source, Callback callback)
    {
        reset();
        auto tokens = m_lexer.tokenize(source);
        debugTokens(tokens);
        callback();

        auto IR = m_parser.parse(tokens);
        debugIR(IR);
        callback();

        auto binary = m_assembler.assemble(IR);

        

        return true;
    }

    template <typename Callback>
    void run(Callback callback)
    {
        while (!m_cpu.halted()) {
            m_cpu.tick();
            callback();
        }
    }

    void halt();
    void reset();
    void debugTokens(const std::vector<Token>& tokens);
    void debugIR(const Program& program);

    const std::vector<std::string>& Errors() const { return m_errors; }

    Buffer m_buffer;

private:
    MipsCPU     m_cpu;
    Lexer       m_lexer;
    Parser      m_parser;
    Assembler   m_assembler;


    std::vector<std::string> m_errors;
};
