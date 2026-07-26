#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <random>
#include <stdexcept>

#include "Memory.h"
#include "Buffer.h"

// Thrown by execute()/fetch()/memoryAccess() for conditions real MIPS hardware
// traps on (signed add/sub/addi overflow) or that indicate a genuine program
// bug rather than normal termination (jumping to / loading from an unmapped
// or misaligned address). `pc` is the address of the offending instruction so
// the GUI can translate it back to a source line via Emulator::m_PCtoLineMap.
struct MipsRuntimeError : std::runtime_error
{
    uint32_t pc;
    MipsRuntimeError(const std::string& msg, uint32_t faultPc)
        : std::runtime_error(msg), pc(faultPc) {}
};

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

    // Backs syscall 32 (Sleep). Left to the GUI layer so it can refresh the
    // display first (so a "reveal, pause, then hide" sequence is actually
    // visible) and keep the UI responsive while waiting, rather than the
    // core blocking the whole app for the duration.
    void setSleepCallback(std::function<void(uint32_t)> cb)
    {
        m_sleepCallback = std::move(cb);
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
    std::function<void(uint32_t)> m_sleepCallback;
    uint32_t m_instr  = 0;
    std::mt19937 m_rng { std::random_device{}() }; // backs syscalls 40/41/42 (seed/random int)

    // Address of the instruction currently flowing through execute()/memoryAccess(),
    // used to attribute a MipsRuntimeError to the right source line. Set by
    // fetch() for the non-pipelined path and by stageEX()/stageMEM() for the
    // pipelined path (where `pc` itself gets temporarily repurposed).
    uint32_t m_currentInstrPC = 0;

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
        uint32_t pc = 0; // instruction address, forwarded so stageMEM() can attribute faults


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