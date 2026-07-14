#pragma once

#include <string>
#include <vector>
#include <array>
#include <functional>

#include "Lexer.h"
#include "Assembler.h"  
#include "MipsCPU.h"
#include "Buffer.h"
#include "Binary.h"
#include "parser.h"
#include "Memory.h"
#include "Loader.h"


class Emulator
{
public:
    Emulator() = default;

    template <typename Callback>
    bool loadProgram(const std::string& source, Callback callback)
    {
        reset();
        auto tokens = m_lexer.tokenize(source);
        //debugTokens(tokens);
        callback();

        auto IR = m_parser.parse(tokens);
        //debugIR(IR);
        callback();

        auto binary = m_assembler.assemble(IR);

        // Load text + data segments into memory at their canonical MIPS addresses
        Loader::load(binary, m_memory, m_cpu.text, m_cpu.data);
        m_cpu.pc      = m_cpu.text;
        m_cpu.textEnd = m_cpu.text + static_cast<uint32_t>(binary.bin.size());
        m_cpu.setMemory(&m_memory);
        m_cpu.setOutput(&m_buffer);
        m_cpu.setInputCallback(m_inputCallback);

        return true;
    }

    template <typename Callback>
    void run(Callback callback, uint32_t maxCycles = 1000000)
    {
        uint32_t cycles = 0;
        while (!m_cpu.halted() && cycles < maxCycles) {
            m_cpu.tick();
            ++cycles;
            callback();
        }
    }

    void halt();
    void reset();
    void debugTokens(const std::vector<Token>& tokens);
    void debugIR(const Program& program);

    void setInputCallback(std::function<std::string(const std::string&)> cb)
    {
        m_inputCallback = std::move(cb);
    }

    // Single-instruction step; no-op if halted or no program loaded.
    void step() { if (!m_cpu.halted()) m_cpu.tick(); }

    bool halted() const { return m_cpu.halted(); }

    std::array<uint32_t, 32> registers() const
    {
        std::array<uint32_t, 32> arr;
        std::copy(std::begin(m_cpu.r), std::end(m_cpu.r), arr.begin());
        return arr;
    }

    uint32_t pc() const { return m_cpu.pc; }

    const std::vector<std::string>& Errors() const { return m_errors; }

    Memory& memory()       { return m_memory; }
    uint32_t textBase() const { return m_cpu.text; }

    Buffer m_buffer;

private:
    MipsCPU     m_cpu;
    Memory      m_memory;
    Lexer       m_lexer;
    Parser      m_parser;
    Assembler   m_assembler;
    std::function<std::string(const std::string&)> m_inputCallback;


    std::vector<std::string> m_errors;
};
