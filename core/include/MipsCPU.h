#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "Memory.h"
#include "Buffer.h"

class MipsCPU
{
public:
    uint32_t r[32] = {};   // general-purpose registers; r[0] always 0
    uint32_t pc     = 0;
    uint32_t text   = 0x00400000;
    uint32_t textEnd = 0;           // one past the last loaded instruction byte
    uint32_t data   = 0x10010000;
    uint32_t stack  = 0x7FFFFFFC;

    void setMemory(Memory* m) { m_mem = m; }
    void setOutput(Buffer*  b) { m_out = b; }

    // Called when the program executes a read syscall.
    // The callback receives a prompt string and returns the user's input.
    void setInputCallback(std::function<std::string(const std::string&)> cb)
    {
        m_inputCallback = std::move(cb);
    }

    bool halted() const { return m_halted; }

    void tick();

private:
    Memory*  m_mem    = nullptr;
    Buffer*  m_out    = nullptr;
    std::function<std::string(const std::string&)> m_inputCallback;
    bool     m_halted = false;
    uint32_t m_instr  = 0;   // raw instruction word from fetch

    // ── Decoded instruction fields (filled by decode()) ───────────────────
    uint32_t m_opcode = 0;
    uint32_t m_rs     = 0;   // source register index
    uint32_t m_rt     = 0;   // target register index
    uint32_t m_rd     = 0;   // destination register index (R-type)
    uint32_t m_shamt  = 0;   // shift amount
    uint32_t m_funct  = 0;   // function code (R-type)
    int32_t  m_imm    = 0;   // sign-extended 16-bit immediate

    // ── Pipeline control signals (filled by execute()) ────────────────────
    uint32_t m_alu_result   = 0;
    uint32_t m_wb_reg       = 0;     // register index to write back to
    bool     m_do_writeback = false; // whether writeBack() stores a result

    void fetch();
    void decode();
    void execute();
    void memoryAccess();
    void writeBack();
    void syscall_handler();
};
