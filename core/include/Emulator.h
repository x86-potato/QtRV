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
#include "InstructionState.h"

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
        while (!m_cpu.halted()) {
            if(m_pipelineMode) {
                m_cpu.cycleTick();
            } else {
                m_cpu.tick();
            }

            //construct the future instruction state for the pipeline panel
            updatePipelineStates();
            
            // If the tick naturally hit a breakpoint, break the run loop
            if (m_cpu.m_breakpointHit) {
                callback();
                break;
            }
            
            ++cycles;
            callback();
        }
    }

// --- PIPELINE TRACKING VARIABLES ---
    int m_globalCycle = 0;
    std::vector<PipelineRow> m_pipelineHistory;
    // Tracks the active instruction index in m_pipelineHistory for: [WB, MEM, EX, ID, IF]
    int m_activeInsts[5] = {-1, -1, -1, -1, -1}; 

    void updatePipelineStates()
    {
        if (!m_pipelineMode) return;
        m_globalCycle++;

        // 1. Log stages completed in the current cycle
        if (m_activeInsts[0] != -1) m_pipelineHistory[m_activeInsts[0]].stages.push_back("WB");
        if (m_activeInsts[1] != -1) m_pipelineHistory[m_activeInsts[1]].stages.push_back("MEM");
        if (m_activeInsts[2] != -1) m_pipelineHistory[m_activeInsts[2]].stages.push_back("EX");
        
        if (m_activeInsts[3] != -1) {
            if (m_cpu.m_stall) m_pipelineHistory[m_activeInsts[3]].stages.push_back("STALL");
            else m_pipelineHistory[m_activeInsts[3]].stages.push_back("ID");
        }
        
        if (m_activeInsts[4] != -1) {
            if (m_cpu.m_stall) m_pipelineHistory[m_activeInsts[4]].stages.push_back("STALL");
            else m_pipelineHistory[m_activeInsts[4]].stages.push_back("IF");
        }

        // 2. Shift the tracker for the next cycle
        m_activeInsts[0] = m_cpu.MEM_WB.valid ? m_activeInsts[1] : -1;
        m_activeInsts[1] = m_cpu.EX_MEM.valid ? m_activeInsts[2] : -1;
        
        if (m_cpu.m_stall) {
            m_activeInsts[2] = -1; // EX gets a bubble
            // ID and IF stay in place
        } else {
            m_activeInsts[2] = m_cpu.ID_EX.valid ? m_activeInsts[3] : -1;
            
            // Check if IF was flushed (invalidated) by a branch in EX
            if (!m_cpu.IF_ID.valid && m_activeInsts[4] != -1) {
                m_pipelineHistory[m_activeInsts[4]].stages.push_back("FLUSH");
                m_activeInsts[3] = -1; // Disappears from the pipeline
            } else {
                m_activeInsts[3] = m_activeInsts[4]; // Moves to ID normally
            }
            
            // 3. Bring a newly fetched instruction into the IF tracker
            if (m_cpu.IF_ID.valid) {
                PipelineRow newRow;
                newRow.pc = m_cpu.IF_ID.pc;
                newRow.lineStr = getInstructionAtPC(m_cpu.IF_ID.pc);
                newRow.startCycle = m_globalCycle; 
                m_pipelineHistory.push_back(newRow);
                
                m_activeInsts[4] = m_pipelineHistory.size() - 1;
            } else {
                m_activeInsts[4] = -1;
            }
        }
    }

    void halt();
    void reset(); 
    void debugTokens(const std::vector<Token>& tokens);
    void debugIR(const Program& program);

    std::string getInstructionAtPC(uint32_t pc) const
    {
        auto it = m_PCtoLineMap.find(pc);
        if (it == m_PCtoLineMap.end()) return "";

        uint32_t line = it->second;
        auto it2 = m_lineToPCMap.find(line);
        if (it2 == m_lineToPCMap.end()) return "";

        uint32_t pcAtLine = it2->second;
        if (pcAtLine != pc) return "";

        // Now we can find the instruction in the assembler's IR
        for (const auto& [irPC, irLine] : m_PCtoLineMap) {
            if (irPC == pc && irLine == line) {
                // We found the matching instruction
                // For simplicity, let's just return a placeholder string
                return std::to_string(irLine);
            }
        }

        return "";
    }


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
        
        if(m_pipelineMode) {
            m_cpu.cycleTick();
        } else {
            m_cpu.tick();
        }

        updatePipelineStates();
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
    std::vector<PipelineRow> m_pipelineStates; // Store the pipeline states for visualization
    bool m_pipelineMode = false;

private:
    MipsCPU     m_cpu;
    Memory      m_memory;
    Lexer       m_lexer;
    Parser      m_parser;
    Assembler   m_assembler;

    std::function<std::string(const std::string&)> m_inputCallback;
    std::vector<std::string> m_errors;

};