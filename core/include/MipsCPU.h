#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>

#include "Memory.h"
#include "Buffer.h"

class MipsCPU
{
public:
    uint32_t r[32] = {};   // general-purpose registers; r[0] always 0
    uint32_t hi = 0;      // high register for mult/div
    uint32_t lo = 0;      // low register for mult/div
    uint32_t pc     = 0;
    uint32_t text   = 0x00400000;
    uint32_t textEnd = 0;           // one past the last loaded instruction byte
    uint32_t data   = 0x10010000;
    uint32_t stack  = 0x7FFFFFFC;

    bool     m_halted = false;
    bool     m_breakpointHit = false;
    bool     m_ignoreNextBreakpoint = false;

    void setMemory(Memory* m) { m_mem = m; }
    void setOutput(Buffer*  b) { m_out = b; }

    void setInputCallback(std::function<std::string(const std::string&)> cb)
    {
        m_inputCallback = std::move(cb);
    }

    bool halted() const { return m_halted; }

    void setBreakpoint(uint32_t pc, bool enabled)
    {
        if (enabled)
            m_breakpoints.insert(pc);
        else
            m_breakpoints.erase(pc);
    }

    std::unordered_set<uint32_t> m_breakpoints;
    void tick();
    void cycleTick(); // For pipeline mode, executes one cycle of the CPU

    Memory*  m_mem    = nullptr;
    Buffer*  m_out    = nullptr;
    std::function<std::string(const std::string&)> m_inputCallback;
    uint32_t m_instr  = 0;

    // ── Decoded instruction fields ───────────────────────────────────────────
    uint32_t m_opcode = 0;
    uint32_t m_rs     = 0;
    uint32_t m_rt     = 0;
    uint32_t m_rd     = 0;
    uint32_t m_shamt  = 0;
    uint32_t m_funct  = 0;
    int32_t  m_imm    = 0;
    uint32_t m_address = 0;

    // ── Pipeline control signals ─────────────────────────────────────────────
    uint32_t m_alu_result   = 0;
    uint32_t m_wb_reg       = 0;
    bool     m_do_writeback = false;
    bool     m_stall = false;

    // --------------------------------------------------------
    // PIPELINE LATCHES (Context-Switching Design)
    // --------------------------------------------------------
    
    struct IF_ID_Latch {
        bool valid = false;
        uint32_t pc = 0; // PC + 4
        uint32_t instruction = 0;
    } IF_ID;

    struct ID_EX_Latch {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t instruction = 0;
        
        // Saved Decode Context
        uint32_t opcode = 0, rs = 0, rt = 0, rd = 0;
        uint32_t shamt = 0, funct = 0, address = 0;
        int32_t  imm = 0;
        uint32_t dest_reg = 0;
    } ID_EX;

    struct EX_MEM_Latch {
        bool valid = false;
        uint32_t opcode = 0;
        uint32_t rt = 0;
        
        // Output from execute()
        uint32_t alu_result = 0;
        uint32_t wb_reg = 0;
        bool do_writeback = false;
        uint32_t dest_reg = 0;
    } EX_MEM;

    struct MEM_WB_Latch {
        bool valid = false;
        // Output from memoryAccess()
        uint32_t alu_result = 0;
        uint32_t wb_reg = 0;
        bool do_writeback = false;
        uint32_t dest_reg = 0;
    } MEM_WB;

    void stageWB();
    void stageMEM();
    void stageEX();
    void stageID();
    void stageIF();

    void fetch();
    void decode();
    void execute();
    void memoryAccess();
    void writeBack();
    void syscall_handler();
};