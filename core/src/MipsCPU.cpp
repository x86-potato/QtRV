#include "MipsCPU.h"
#include <string>

// ── Helpers ───────────────────────────────────────────────────────────────────
// signed(x) : treat a uint32_t as its signed equivalent for arithmetic
// uimm()    : zero-extended 16-bit immediate (used by andi/ori/xori)
// branch()  : compute branch target from the sign-extended offset

static inline int32_t  signed32(uint32_t x)   { return static_cast<int32_t>(x); }
static inline uint32_t uimm(uint32_t instr)    { return instr & 0xFFFF; }

// ── Tick ─────────────────────────────────────────────────────────────────────

void MipsCPU::tick()
{
    if (m_halted) return;
    fetch();
    if (m_halted) return;   // fetch() may halt on unmapped PC
    decode();
    execute();
    memoryAccess();
    writeBack();
}

// ── Stage 1 : Fetch ───────────────────────────────────────────────────────────
// Read the 32-bit instruction at PC and advance PC by 4.
// Halts the CPU if the PC points to unmapped memory (program ran off the end).

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
// Crack the instruction word into named fields.
// All downstream stages read these members; nobody re-masks m_instr.

void MipsCPU::decode()
{
    m_opcode = (m_instr >> 26) & 0x3F;
    m_rs     = (m_instr >> 21) & 0x1F;
    m_rt     = (m_instr >> 16) & 0x1F;
    m_rd     = (m_instr >> 11) & 0x1F;
    m_shamt  = (m_instr >>  6) & 0x1F;
    m_funct  =  m_instr        & 0x3F;
    m_imm    = static_cast<int32_t>(static_cast<int16_t>(m_instr & 0xFFFF));

    // Clear pipeline signals so execute() only sets what it needs.
    m_alu_result   = 0;
    m_wb_reg       = 0;
    m_do_writeback = false;
}

// ── Stage 3 : Execute ─────────────────────────────────────────────────────────
// Compute the ALU result. Branches update PC directly.
// Loads/stores only compute the effective address here.

void MipsCPU::execute()
{
    // wb() schedules a register write-back — keeps each case to one line.
    auto wb = [&](uint32_t reg, uint32_t val)
    {
        m_alu_result   = val;
        m_wb_reg       = reg;
        m_do_writeback = true;
    };

    // branch() computes the absolute target address.
    // fetch() already advanced pc past the branch (+4), so:
    //   target = pc_current + offset * 4
    //          = (branch_addr + 4) + offset * 4   — matches the MIPS spec exactly
    auto branch = [&]() -> uint32_t
    {
        return pc + static_cast<uint32_t>(m_imm << 2);
    };

    // ── R-type (opcode == 0) ─────────────────────────────────────────────────
    if (m_opcode == 0)
    {
        switch (m_funct)
        {
        case 0x00: wb(m_rd, r[m_rt] << m_shamt);                           break; // sll
        case 0x02: wb(m_rd, r[m_rt] >> m_shamt);                           break; // srl
        case 0x03: wb(m_rd, static_cast<uint32_t>(signed32(r[m_rt]) >> m_shamt)); break; // sra
        case 0x04: wb(m_rd, r[m_rt] << (r[m_rs] & 0x1F));                  break; // sllv
        case 0x06: wb(m_rd, r[m_rt] >> (r[m_rs] & 0x1F));                  break; // srlv
        case 0x07: wb(m_rd, static_cast<uint32_t>(signed32(r[m_rt]) >> (r[m_rs] & 0x1F))); break; // srav

        case 0x08: pc = r[m_rs];                                            break; // jr
        case 0x09: wb(m_rd ? m_rd : 31, pc); pc = r[m_rs];                 break; // jalr

        case 0x0C: syscall_handler();                                       break; // syscall

        case 0x20: // add
        case 0x21: wb(m_rd, r[m_rs] + r[m_rt]);                            break; // addu
        case 0x22: // sub
        case 0x23: wb(m_rd, r[m_rs] - r[m_rt]);                            break; // subu
        case 0x24: wb(m_rd, r[m_rs] & r[m_rt]);                            break; // and
        case 0x25: wb(m_rd, r[m_rs] | r[m_rt]);                            break; // or
        case 0x26: wb(m_rd, r[m_rs] ^ r[m_rt]);                            break; // xor
        case 0x27: wb(m_rd, ~(r[m_rs] | r[m_rt]));                         break; // nor

        case 0x2A: wb(m_rd, signed32(r[m_rs]) < signed32(r[m_rt]) ? 1u:0u); break; // slt
        case 0x2B: wb(m_rd, r[m_rs] < r[m_rt]                      ? 1u:0u); break; // sltu

        default: break;
        }
        return;
    }

    // ── I-type ───────────────────────────────────────────────────────────────
    switch (m_opcode)
    {
    // Arithmetic
    case 0x08: // addi
    case 0x09: wb(m_rt, static_cast<uint32_t>(signed32(r[m_rs]) + m_imm)); break; // addiu

    // Comparisons
    case 0x0A: wb(m_rt, signed32(r[m_rs]) < m_imm                  ? 1u:0u); break; // slti
    case 0x0B: wb(m_rt, r[m_rs] < static_cast<uint32_t>(m_imm)     ? 1u:0u); break; // sltiu

    // Bitwise  (andi/ori/xori use the zero-extended immediate)
    case 0x0C: wb(m_rt, r[m_rs] & uimm(m_instr));  break; // andi
    case 0x0D: wb(m_rt, r[m_rs] | uimm(m_instr));  break; // ori
    case 0x0E: wb(m_rt, r[m_rs] ^ uimm(m_instr));  break; // xori
    case 0x0F: wb(m_rt, uimm(m_instr) << 16);       break; // lui

    // Branches
    case 0x04: if (r[m_rs] == r[m_rt])           pc = branch(); break; // beq
    case 0x05: if (r[m_rs] != r[m_rt])           pc = branch(); break; // bne
    case 0x06: if (signed32(r[m_rs]) <= 0)        pc = branch(); break; // blez
    case 0x07: if (signed32(r[m_rs]) >  0)        pc = branch(); break; // bgtz

    // Loads and stores: compute effective address; memory stage does the rest.
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x30:
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2E: case 0x38:
        m_alu_result = static_cast<uint32_t>(signed32(r[m_rs]) + m_imm);
        break;

    default: break;
    }
}

// ── Stage 4 : Memory Access ───────────────────────────────────────────────────
// Loads read a value and schedule a write-back.
// Stores write to memory. ALU instructions pass through untouched.

void MipsCPU::memoryAccess()
{

    // load() schedules a register write-back with the loaded value.
    auto load = [&](uint32_t val)
    {
        m_alu_result   = val;
        m_wb_reg       = m_rt;
        m_do_writeback = true;
    };

    switch (m_opcode)
    {
    // Loads
    case 0x23: if (auto *p = m_mem->readWord    (m_alu_result)) load(*p);                                             break; // lw
    case 0x20: if (auto *p = m_mem->readByte    (m_alu_result)) load(static_cast<uint32_t>(signed32(*p << 24) >> 24)); break; // lb  (sign)
    case 0x24: if (auto *p = m_mem->readByte    (m_alu_result)) load(*p);                                             break; // lbu
    case 0x21: if (auto *p = m_mem->readHalfword(m_alu_result)) load(static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(*p)))); break; // lh  (sign)
    case 0x25: if (auto *p = m_mem->readHalfword(m_alu_result)) load(*p);                                             break; // lhu

    // Stores
    case 0x2B: m_mem->writeWord    (m_alu_result, r[m_rt]);                         break; // sw
    case 0x28: m_mem->writeByte    (m_alu_result, static_cast<uint8_t> (r[m_rt]));  break; // sb
    case 0x29: m_mem->writeHalfword(m_alu_result, static_cast<uint16_t>(r[m_rt]));  break; // sh

    default: break;
    }
}

// ── Stage 5 : Write-Back ─────────────────────────────────────────────────────
// Commit the result to the register file.
//  is hardwired to 0 and is always restored last.

void MipsCPU::writeBack()
{
    if (m_do_writeback && m_wb_reg != 0)
        r[m_wb_reg] = m_alu_result;

    r[0] = 0;
}

// ── Syscall handler ───────────────────────────────────────────────────────────

void MipsCPU::syscall_handler()
{
    switch (r[2])   // $v0 holds the syscall code
    {
    case 1:     // print_int  — value in $a0
        if (m_out)
            m_out->m_data += std::to_string(signed32(r[4]));
        break;

    case 4:     // print_string  — address of null-terminated string in $a0
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

    case 5:     // read_int  — result returned in $v0
        if (m_inputCallback)
        {
            std::string raw = m_inputCallback("Enter integer:");
            try   { r[2] = static_cast<uint32_t>(std::stoi(raw, nullptr, 0)); }
            catch (...) { r[2] = 0; }
        }
        break;

    case 8:     // read_string  — $a0 = buffer address, $a1 = max length
        if (m_inputCallback && m_mem)
        {
            std::string input = m_inputCallback("Enter string:");
            uint32_t addr   = r[4];
            uint32_t maxLen = r[5];
            uint32_t i = 0;
            for (; i < maxLen - 1 && i < static_cast<uint32_t>(input.size()); ++i)
                m_mem->writeByte(addr + i, static_cast<uint8_t>(input[i]));
            m_mem->writeByte(addr + i, 0);   // null terminator
        }
        break;

    case 10:    // exit
        m_halted = true;
        break;

    default: break;
    }
}
