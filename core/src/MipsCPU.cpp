#include "MipsCPU.h"
#include <string>

// ── Helpers ───────────────────────────────────────────────────────────────────
static inline int32_t   signed32(uint32_t x)   { return static_cast<int32_t>(x); }
static inline uint32_t uimm(uint32_t instr)    { return instr & 0xFFFF; }

// ── Tick ─────────────────────────────────────────────────────────────────────

void MipsCPU::tick()
{
    if (m_halted) return;

    // Check for breakpoint BEFORE we fetch the instruction.
    // If we just hit step/resume on a breakpoint, m_ignoreNextBreakpoint allows us to execute it.
    if (m_breakpoints.find(pc) != m_breakpoints.end() && !m_ignoreNextBreakpoint)
    {
        m_halted = true;
        m_breakpointHit = true;
        return; // Fully halt tick here so we don't execute junk.
    }
    
    // Clear the ignore flag so subsequent breakpoints work normally
    m_ignoreNextBreakpoint = false;

    fetch();
    if (m_halted) return;   // fetch() may halt on unmapped PC
    decode();
    execute();
    memoryAccess();
    writeBack();
}

// ── Stage 1 : Fetch ───────────────────────────────────────────────────────────

void MipsCPU::fetch()
{
    if (!m_mem) { m_halted = true; return; }
    // Halt if PC has run off the end of the loaded text segment
    if (textEnd != 0 && pc >= textEnd) { m_halted = true; return; }
    uint32_t *word = m_mem->readWord(pc);
    if (!word) { m_halted = true; return; }   // PC outside mapped memory
    m_instr = *word;
    pc += 4;
}

// ── Stage 2 : Decode ──────────────────────────────────────────────────────────

void MipsCPU::decode()
{
    m_opcode = (m_instr >> 26) & 0x3F;
    m_rs     = (m_instr >> 21) & 0x1F;
    m_rt     = (m_instr >> 16) & 0x1F;
    m_rd     = (m_instr >> 11) & 0x1F;
    m_shamt  = (m_instr >>  6) & 0x1F;
    m_funct  =  m_instr        & 0x3F;
    m_imm    = static_cast<int32_t>(static_cast<int16_t>(m_instr & 0xFFFF));
    m_address = (m_instr & 0x3FFFFFF);

    // Clear pipeline signals so execute() only sets what it needs.
    m_alu_result   = 0;
    m_wb_reg       = 0;
    m_do_writeback = false;
}

// ── Stage 3 : Execute ─────────────────────────────────────────────────────────

void MipsCPU::execute()
{
    // wr schedules a register write-back
    auto wr = [&](uint32_t reg, uint32_t val)
    {
        m_alu_result   = val;
        m_wb_reg       = reg;
        m_do_writeback = true;
    };

    auto branch = [&]() -> uint32_t
    {
        return pc + static_cast<uint32_t>(m_imm << 2);
    };

    auto j_branch = [&]() -> uint32_t
    {
        return (m_address << 2);
    };

    // ── R-type (opcode == 0) ─────────────────────────────────────────────────
    if (m_opcode == 0)
    {
        switch (m_funct)
        {
        case 0x00: wr(m_rd, r[m_rt] << m_shamt);                           break; // sll
        case 0x02: wr(m_rd, r[m_rt] >> m_shamt);                           break; // srl
        case 0x03: wr(m_rd, static_cast<uint32_t>(signed32(r[m_rt]) >> m_shamt)); break; // sra
        case 0x04: wr(m_rd, r[m_rt] << (r[m_rs] & 0x1F));                  break; // sllv
        case 0x06: wr(m_rd, r[m_rt] >> (r[m_rs] & 0x1F));                  break; // srlv
        case 0x07: wr(m_rd, static_cast<uint32_t>(signed32(r[m_rt]) >> (r[m_rs] & 0x1F))); break; // srav

        case 0x08: pc = r[m_rs];                                           break; // jr
        case 0x09: wr(m_rd ? m_rd : 31, pc); pc = r[m_rs];                 break; // jalr

        case 0x10: wr(m_rd, hi);                                        break; // mfhi
        case 0x12: wr(m_rd, lo);                                        break; // mflo

        case 0x0C: syscall_handler();                                       break; // syscall

        case 0x18: { // mult
            int64_t prod = static_cast<int64_t>(signed32(r[m_rs])) * static_cast<int64_t>(signed32(r[m_rt]));
            hi = static_cast<uint32_t>(prod >> 32);
            lo = static_cast<uint32_t>(prod & 0xFFFFFFFF);
        } break;
        case 0x20: // add
        case 0x21: wr(m_rd, r[m_rs] + r[m_rt]);                            break; // addu
        case 0x22: // sub
        case 0x23: wr(m_rd, r[m_rs] - r[m_rt]);                            break; // subu
        case 0x24: wr(m_rd, r[m_rs] & r[m_rt]);                            break; // and
        case 0x25: wr(m_rd, r[m_rs] | r[m_rt]);                            break; // or
        case 0x26: wr(m_rd, r[m_rs] ^ r[m_rt]);                            break; // xor
        case 0x27: wr(m_rd, ~(r[m_rs] | r[m_rt]));                         break; // nor

        case 0x2A: wr(m_rd, signed32(r[m_rs]) < signed32(r[m_rt]) ? 1u:0u); break; // slt
        case 0x2B: wr(m_rd, r[m_rs] < r[m_rt]                      ? 1u:0u); break; // sltu

        default: break;
        }
        return;
    }

    // ── I-type ───────────────────────────────────────────────────────────────
    switch (m_opcode)
    {
    // Arithmetic
    case 0x08: // addi
    case 0x09: wr(m_rt, static_cast<uint32_t>(signed32(r[m_rs]) + m_imm)); break; // addiu

    // Comparisons
    case 0x0A: wr(m_rt, signed32(r[m_rs]) < m_imm                  ? 1u:0u); break; // slti
    case 0x0B: wr(m_rt, r[m_rs] < static_cast<uint32_t>(m_imm)     ? 1u:0u); break; // sltiu

    // Bitwise
    case 0x0C: wr(m_rt, r[m_rs] & uimm(m_instr));  break; // andi
    case 0x0D: wr(m_rt, r[m_rs] | uimm(m_instr));  break; // ori
    case 0x0E: wr(m_rt, r[m_rs] ^ uimm(m_instr));  break; // xori
    case 0x0F: wr(m_rt, uimm(m_instr) << 16);       break; // lui

    // Branches
    case 0x04: if (r[m_rs] == r[m_rt])           pc = branch(); break; // beq
    case 0x05: if (r[m_rs] != r[m_rt])           pc = branch(); break; // bne
    case 0x06: if (signed32(r[m_rs]) <= 0)        pc = branch(); break; // blez
    case 0x07: if (signed32(r[m_rs]) >  0)        pc = branch(); break; // bgtz

    // Loads and stores
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x30:
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2E: case 0x38:
        m_alu_result = static_cast<uint32_t>(signed32(r[m_rs]) + m_imm);
        break;

    default: break;
    }

    // J type handle
    switch(m_opcode)
    {
        case 0x02:              // J instruction
            pc = j_branch(); break;

        case 0x03:              // JAL instruction
            wr(31, pc);        // Store return address in $ra
            pc = j_branch(); break;
    }
}

// ── Stage 4 : Memory Access ───────────────────────────────────────────────────

void MipsCPU::memoryAccess()
{
    auto load = [&](uint32_t val)
    {
        m_alu_result   = val;
        m_wb_reg       = m_rt;
        m_do_writeback = true;
    };

    switch (m_opcode)
    {
    // Loads
    case 0x23: if (auto *p = m_mem->readWord    (m_alu_result)) load(*p);                                            break; // lw
    case 0x20: if (auto *p = m_mem->readByte    (m_alu_result)) load(static_cast<uint32_t>(signed32(*p << 24) >> 24)); break; // lb  (sign)
    case 0x24: if (auto *p = m_mem->readByte    (m_alu_result)) load(*p);                                            break; // lbu
    case 0x21: if (auto *p = m_mem->readHalfword(m_alu_result)) load(static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(*p)))); break; // lh  (sign)
    case 0x25: if (auto *p = m_mem->readHalfword(m_alu_result)) load(*p);                                            break; // lhu

    // Stores
    case 0x2B: m_mem->writeWord    (m_alu_result, r[m_rt]);                         break; // sw
    case 0x28: m_mem->writeByte    (m_alu_result, static_cast<uint8_t> (r[m_rt]));  break; // sb
    case 0x29: m_mem->writeHalfword(m_alu_result, static_cast<uint16_t>(r[m_rt]));  break; // sh

    default: break;
    }
}

// ── Stage 5 : Write-Back ─────────────────────────────────────────────────────

void MipsCPU::writeBack()
{
    if (m_do_writeback && m_wb_reg != 0)
        r[m_wb_reg] = m_alu_result;

    r[0] = 0;
}

// ── Syscall handler ───────────────────────────────────────────────────────────

void MipsCPU::syscall_handler()
{
    switch (r[2]) 
    {
    case 1: 
        if (m_out)
            m_out->m_data += std::to_string(signed32(r[4]));
        break;
    case 4: 
        if (m_mem && m_out)
        {
            uint32_t addr = r[4];
            while (uint8_t *b = m_mem->readByte(addr++))
            {
                if (*b == 0) break;
                m_out->m_data += static_cast<char>(*b);
            }
        }
        break;
    case 5:
        if (m_inputCallback)
        {
            std::string raw = m_inputCallback("Enter integer:");
            try   { r[2] = static_cast<uint32_t>(std::stoi(raw, nullptr, 0)); }
            catch (...) { r[2] = 0; }
        }
        break;
    case 8: 
        if (m_inputCallback && m_mem)
        {
            std::string input = m_inputCallback("Enter string:");
            uint32_t addr   = r[4];
            uint32_t maxLen = r[5];
            uint32_t i = 0;
            for (; i < maxLen - 1 && i < static_cast<uint32_t>(input.size()); ++i)
                m_mem->writeByte(addr + i, static_cast<uint8_t>(input[i]));
            m_mem->writeByte(addr + i, 0); 
        }
        break;
    case 10: 
        m_halted = true;
        break;
    default: break;
    }
}


// ═════════════════════════════════════════════════════════════════════════════
// ── PIPELINE ARCHITECTURE (Cycle Accurate Datapath) ──────────────────────────
// ═════════════════════════════════════════════════════════════════════════════

void MipsCPU::cycleTick() 
{
    // If CPU is fully halted and all pipeline latches have drained, stop completely
    if (m_halted && !IF_ID.valid && !ID_EX.valid && !EX_MEM.valid && !MEM_WB.valid) return;

    m_stall = false;

    // Evaluate stages RIGHT to LEFT
    stageWB();
    stageMEM();
    stageEX();
    stageID();
    
    // Only fetch if no hazard stall occurred AND the CPU hasn't halted (e.g. from EOF or syscall 10)
    if (!m_stall && !m_halted) {
        stageIF();
    }
}

void MipsCPU::stageWB() {
    if (!MEM_WB.valid) return;
    
    // Restore context and use existing writeBack logic
    m_do_writeback = MEM_WB.do_writeback;
    m_wb_reg       = MEM_WB.wb_reg;
    m_alu_result   = MEM_WB.alu_result;
    
    writeBack();
}

void MipsCPU::stageMEM() {
    MEM_WB.valid = EX_MEM.valid;
    if (!EX_MEM.valid) return;

    m_opcode       = EX_MEM.opcode;
    m_rt           = EX_MEM.rt;
    m_alu_result   = EX_MEM.alu_result;
    m_wb_reg       = EX_MEM.wb_reg;
    m_do_writeback = EX_MEM.do_writeback;

    memoryAccess();

    MEM_WB.alu_result   = m_alu_result;
    MEM_WB.wb_reg       = m_wb_reg;
    MEM_WB.do_writeback = m_do_writeback;
    
    // Pass the hazard tracker forward
    MEM_WB.dest_reg     = EX_MEM.dest_reg; 
}

void MipsCPU::stageEX() {
    EX_MEM.valid = ID_EX.valid;
    if (!ID_EX.valid) return;

    m_instr   = ID_EX.instruction;
    m_opcode  = ID_EX.opcode;
    m_rs      = ID_EX.rs;
    m_rt      = ID_EX.rt;
    m_rd      = ID_EX.rd;
    m_shamt   = ID_EX.shamt;
    m_funct   = ID_EX.funct;
    m_imm     = ID_EX.imm;
    m_address = ID_EX.address;
    
    uint32_t fetch_pc = pc; 
    pc = ID_EX.pc; 
    
    m_alu_result = 0;
    m_wb_reg = 0;
    m_do_writeback = false;

    execute(); 

    if (pc != ID_EX.pc) {
        IF_ID.valid = false;
        m_halted = false;
    } else {
        pc = fetch_pc; 
    }

    EX_MEM.opcode       = m_opcode;
    EX_MEM.rt           = m_rt;
    EX_MEM.alu_result   = m_alu_result;
    EX_MEM.wb_reg       = m_wb_reg;
    EX_MEM.do_writeback = m_do_writeback;
    
    // Pass the hazard tracker forward
    EX_MEM.dest_reg     = ID_EX.dest_reg; 
}

void MipsCPU::stageID() {
    if (!IF_ID.valid) {
        ID_EX.valid = false;
        return;
    }

    m_instr = IF_ID.instruction;
    decode(); 

    // --- HAZARD DETECTION ---
    bool hazard = false;
    bool reads_rs = true;
    bool reads_rt = true;
    
    if (m_opcode == 0x02 || m_opcode == 0x03 || m_opcode == 0x0F) reads_rs = false;
    if (m_opcode != 0x00 && m_opcode != 0x2B && m_opcode != 0x28 && m_opcode != 0x29 && 
        m_opcode != 0x04 && m_opcode != 0x05 && m_opcode != 0x06 && m_opcode != 0x07) {
        reads_rt = false; 
    }

    // Identify where this instruction will write data
    uint32_t dest_reg = 0;
    if (m_opcode == 0x00) dest_reg = m_rd; // R-type
    else if (m_opcode == 0x03) dest_reg = 31; // JAL
    else if (m_opcode != 0x2B && m_opcode != 0x28 && m_opcode != 0x29 && 
             m_opcode != 0x04 && m_opcode != 0x05 && m_opcode != 0x06 && m_opcode != 0x07 && m_opcode != 0x02) {
        dest_reg = m_rt; // Loads and I-Type arithmetic
    }
    
    // Stall if EX or MEM stages are computing a register we need
    if (EX_MEM.valid && EX_MEM.dest_reg != 0) {
        if (reads_rs && EX_MEM.dest_reg == m_rs) hazard = true;
        if (reads_rt && EX_MEM.dest_reg == m_rt) hazard = true;
    }
    if (MEM_WB.valid && MEM_WB.dest_reg != 0) {
        if (reads_rs && MEM_WB.dest_reg == m_rs) hazard = true;
        if (reads_rt && MEM_WB.dest_reg == m_rt) hazard = true;
    }
    
    if (m_opcode == 0x00 && m_funct == 0x0C) {
        if (EX_MEM.valid || MEM_WB.valid) hazard = true;
    }

    if (hazard) {
        ID_EX.valid = false;
        m_stall = true;      
        return;
    }

    ID_EX.valid   = true;
    ID_EX.pc      = IF_ID.pc;
    ID_EX.instruction = m_instr;
    ID_EX.opcode  = m_opcode;
    ID_EX.rs      = m_rs;
    ID_EX.rt      = m_rt;
    ID_EX.rd      = m_rd;
    ID_EX.shamt   = m_shamt;
    ID_EX.funct   = m_funct;
    ID_EX.imm     = m_imm;
    ID_EX.address = m_address;
    
    // Track hazard target
    ID_EX.dest_reg = dest_reg;
}

void MipsCPU::stageIF() {
    fetch(); // Modifies m_instr, advances pc by 4, and may set m_halted
    
    if (m_halted) {
        // We reached EOF or unmapped memory. Invalidate fetch so pipeline can drain.
        IF_ID.valid = false;
        return;
    }

    IF_ID.instruction = m_instr;
    IF_ID.pc          = pc; 
    IF_ID.valid       = true;
}