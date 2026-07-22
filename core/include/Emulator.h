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

using PCtoLine = std::map<uint32_t, uint32_t>; 
using LinetoPC = std::map<uint32_t, uint32_t>; 

class Emulator
{
public:
    Emulator() = default;

    template <typename Callback>
    bool loadProgram(const std::string& source, Callback callback)
    {
        reset();
        auto tokens = m_lexer.tokenize(source);
        callback();

        auto IR = m_parser.parse(tokens);
        callback();

        auto binary = m_assembler.assemble(IR);
        m_PCtoLineMap = std::move(m_assembler.PCtoLine);
        m_lineToPCMap = std::move(m_assembler.LinetoPC);

        Loader::load(binary, m_memory, m_cpu.text, m_cpu.data);
        m_cpu.pc      = m_cpu.text;
        m_cpu.textEnd = m_cpu.text + static_cast<uint32_t>(binary.bin.size());
        m_cpu.setMemory(&m_memory);
        m_cpu.setOutput(&m_buffer);
        m_cpu.setInputCallback(m_inputCallback);

        InitBreakpoints();

        return true;
    }

    template <typename Callback>
    void run(Callback callback, uint32_t maxCycles = UINT32_MAX)
    {
        // Don't run if the program ended naturally
        if (m_cpu.halted() && !m_cpu.m_breakpointHit) return;

        // If we are parked on a breakpoint, configure CPU to step past it
        if (m_cpu.m_breakpointHit) {
            m_cpu.m_halted = false;
            m_cpu.m_breakpointHit = false;
            m_cpu.m_ignoreNextBreakpoint = true;
        }

        uint32_t cycles = 0;
        while (!m_cpu.halted() && cycles < maxCycles) {
            m_cpu.tick();
            
            // If the tick naturally hit a breakpoint, break the run loop
            if (m_cpu.m_breakpointHit) {
                callback();
                break;
            }
            
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

    void step() { 
        if (m_cpu.halted() && !m_cpu.m_breakpointHit) return; // Ignore if program finished naturally

        // If stepping from a breakpoint, step past it
        if (m_cpu.m_breakpointHit) {
            m_cpu.m_breakpointHit = false;
            m_cpu.m_halted = false;
            m_cpu.m_ignoreNextBreakpoint = true;
        } 
        
        m_cpu.tick(); 
    }

    void setBreakpoint(uint32_t pc, bool enabled);

    void InitBreakpoints()
    {
        for (const auto& line : m_breakpoint_lines)
        {
            if (m_lineToPCMap.find(line+1) != m_lineToPCMap.end())
            {
                uint32_t pc = m_lineToPCMap[line+1];
                m_cpu.setBreakpoint(pc, true);
            }
        }
    }
    
    bool halted() const { return m_cpu.halted(); }
    bool isFinished() const { return m_cpu.halted() && !m_cpu.m_breakpointHit; }
    bool isBreakpoint() const { return m_cpu.m_breakpointHit; }

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
    PCtoLine    m_PCtoLineMap; 
    LinetoPC    m_lineToPCMap;
    std::unordered_set<uint32_t> m_breakpoint_lines; 

private:
    MipsCPU     m_cpu;
    Memory      m_memory;
    Lexer       m_lexer;
    Parser      m_parser;
    Assembler   m_assembler;

    std::function<std::string(const std::string&)> m_inputCallback;
    std::vector<std::string> m_errors;
};