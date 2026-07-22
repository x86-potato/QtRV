#include <Assembler.h>
#include <cstdint>
#include <stdexcept>

// Maps opcode name -> encoding descriptor.
// rs_idx / rt_idx == -1 means the field is implicit zero.
//
// Operand index conventions (as stored by the parser):
//   Arithmetic/logic  op rt, rs, imm     -> [rt=0, rs=1, imm=2]
//   lui               lui rt, imm        -> [rt=0, imm=1]          rs implicit 0
//   Branch (2-reg)    beq rs, rt, off    -> [rs=0, rt=1, imm=2]
//   Branch (1-reg)    blez rs, off       -> [rs=0, imm=1]          rt implicit 0
//   Load/Store        lw rt, imm(rs)     -> [rt=0, imm=1, rs=2]   (parser flattens parens)
static const std::unordered_map<std::string, InstrDesc> InstrFormatMap =
{
    // Branches
    {"beq",   {InstrType::I_TYPE, /*rs*/0, /*rt*/1, /*imm*/2}},
    {"bne",   {InstrType::I_TYPE, /*rs*/0, /*rt*/1, /*imm*/2}},
    {"blez",  {InstrType::I_TYPE, /*rs*/0, /*rt*/-1, /*imm*/1}},
    {"bgtz",  {InstrType::I_TYPE, /*rs*/0, /*rt*/-1, /*imm*/1}},
    // Arithmetic / logic
    {"addi",  {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"addiu", {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"slti",  {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"sltiu", {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"andi",  {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"ori",   {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"xori",  {InstrType::I_TYPE, /*rs*/1, /*rt*/0, /*imm*/2}},
    {"lui",   {InstrType::I_TYPE, /*rs*/-1, /*rt*/0, /*imm*/1}},
    // Loads  (rt, imm(rs))
    {"lb",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"lh",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"lwl",   {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"lw",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"lbu",   {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"lhu",   {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"lwr",   {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"ll",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    // Stores  (rt, imm(rs))
    {"sb",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"sh",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"swl",   {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"sw",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"swr",   {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},
    {"sc",    {InstrType::I_TYPE, /*rs*/2, /*rt*/0, /*imm*/1}},

    // ── R-type ────────────────────────────────────────────────────────────
    // Operand order for 3-reg arithmetic: op rd, rs, rt  -> [rd=0, rs=1, rt=2]
    // Operand order for shifts:           op rd, rt, sa  -> [rd=0, rt=1, sa=2]
    // Operand order for jr/jalr:          op rs          -> [rs=0]
    // syscall / break have no register operands.
    //
    //                                          rs    rt   imm   rd   shamt funct
    {"syscall", {InstrType::R_TYPE,             -1,   -1,  -1,  -1,   -1, 0x0C}},
    {"break",   {InstrType::R_TYPE,             -1,   -1,  -1,  -1,   -1, 0x0D}},
    // Arithmetic / logic
    {"add",     {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x20}},
    {"addu",    {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x21}},
    {"sub",     {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x22}},
    {"subu",    {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x23}},
    {"and",     {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x24}},
    {"or",      {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x25}},
    {"xor",     {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x26}},
    {"nor",     {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x27}},
    {"slt",     {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x2A}},
    {"sltu",    {InstrType::R_TYPE, /*rs*/1, /*rt*/2, -1, /*rd*/0,   -1, 0x2B}},
    // Multiply / divide (write to HI/LO; no rd)
    {"mult",    {InstrType::R_TYPE, /*rs*/0, /*rt*/1, -1,    -1,      -1, 0x18}},
    {"multu",   {InstrType::R_TYPE, /*rs*/0, /*rt*/1, -1,    -1,      -1, 0x19}},
    {"div",     {InstrType::R_TYPE, /*rs*/0, /*rt*/1, -1,    -1,      -1, 0x1A}},
    {"divu",    {InstrType::R_TYPE, /*rs*/0, /*rt*/1, -1,    -1,      -1, 0x1B}},
    // HI/LO moves (rd only)
    {"mfhi",    {InstrType::R_TYPE,    -1,      -1,   -1, /*rd*/0,   -1, 0x10}},
    {"mthi",    {InstrType::R_TYPE, /*rs*/0,    -1,   -1,    -1,      -1, 0x11}},
    {"mflo",    {InstrType::R_TYPE,    -1,      -1,   -1, /*rd*/0,   -1, 0x12}},
    {"mtlo",    {InstrType::R_TYPE, /*rs*/0,    -1,   -1,    -1,      -1, 0x13}},
    // Shifts: op rd, rt, sa
    {"sll",     {InstrType::R_TYPE,    -1,  /*rt*/1,  -1, /*rd*/0, /*sa*/2, 0x00}},
    {"srl",     {InstrType::R_TYPE,    -1,  /*rt*/1,  -1, /*rd*/0, /*sa*/2, 0x02}},
    {"sra",     {InstrType::R_TYPE,    -1,  /*rt*/1,  -1, /*rd*/0, /*sa*/2, 0x03}},
    // Variable shifts: op rd, rt, rs
    {"sllv",    {InstrType::R_TYPE, /*rs*/2, /*rt*/1,  -1, /*rd*/0,   -1, 0x04}},
    {"srlv",    {InstrType::R_TYPE, /*rs*/2, /*rt*/1,  -1, /*rd*/0,   -1, 0x06}},
    {"srav",    {InstrType::R_TYPE, /*rs*/2, /*rt*/1,  -1, /*rd*/0,   -1, 0x07}},
    // Jumps
    {"jr",      {InstrType::R_TYPE, /*rs*/0,    -1,   -1,    -1,      -1, 0x08}},
    {"jalr",    {InstrType::R_TYPE, /*rs*/1,    -1,   -1, /*rd*/0,   -1, 0x09}},
};

static void pushWord(Binary &bin, uint32_t word)
{
    bin.bin.push_back( word        & 0xFF);
    bin.bin.push_back((word >>  8) & 0xFF);
    bin.bin.push_back((word >> 16) & 0xFF);
    bin.bin.push_back((word >> 24) & 0xFF);
}

// Parse an integer literal that may be decimal or 0x-prefixed hex.
static int32_t parseImm(const std::string &val, const std::string &opcode, int idx)
{
    if (val.empty())
        throw std::runtime_error("'" + opcode + "': operand [" + std::to_string(idx) + "] is empty");
    try {
        return static_cast<int32_t>(std::stoi(val, nullptr, 0));
    } catch (const std::invalid_argument &) {
        throw std::runtime_error("'" + opcode + "': operand [" + std::to_string(idx)
            + "] '" + val + "' is not a valid integer immediate");
    } catch (const std::out_of_range &) {
        throw std::runtime_error("'" + opcode + "': operand [" + std::to_string(idx)
            + "] '" + val + "' is out of integer range");
    }
}

static uint32_t encodeRType(const Instruction &ir, const InstrDesc &desc)
{
    uint32_t rs    = 0;
    if (desc.rs_idx >= 0)
        rs = TokenToBinary(ir.operands[desc.rs_idx].value).value;

    uint32_t rt    = 0;
    if (desc.rt_idx >= 0)
        rt = TokenToBinary(ir.operands[desc.rt_idx].value).value;

    uint32_t rd    = 0;
    if (desc.rd_idx >= 0)
        rd = TokenToBinary(ir.operands[desc.rd_idx].value).value;

    uint32_t shamt = 0;
    if (desc.shamt_idx >= 0)
        shamt = static_cast<uint32_t>(parseImm(ir.operands[desc.shamt_idx].value, ir.opcode, desc.shamt_idx)) & 0x1F;

    // opcode is always 0 for R-type
    return (rs << 21) | (rt << 16) | (rd << 11) | (shamt << 6) | desc.funct;
}

static uint32_t encodeIType(const Instruction &ir, const InstrDesc &desc,
                             const std::unordered_map<std::string, int32_t> &labelMap,
                             int32_t instrAbsAddr)
{
    BinaryMap op = TokenToBinary(ir.opcode);

    uint32_t rs_val = 0;
    if (desc.rs_idx >= 0)
        rs_val = TokenToBinary(ir.operands[desc.rs_idx].value).value;

    uint32_t rt_val = 0;
    if (desc.rt_idx >= 0)
        rt_val = TokenToBinary(ir.operands[desc.rt_idx].value).value;

    // Resolve the immediate: raw integer or a label reference (branch target).
    int32_t immVal = 0;
    const Token &immToken = ir.operands[desc.imm_idx];
    if (immToken.type == TokenType::IDENTIFIER)
    {
        auto it = labelMap.find(immToken.value);
        if (it == labelMap.end())
            throw std::runtime_error("'" + ir.opcode + "': undefined label '" + immToken.value + "'");
        // MIPS branch offset = (target_abs - (instr_abs + 4)) / 4
        immVal = (it->second - (instrAbsAddr + 4)) / 4;
    }
    else
    {
        immVal = parseImm(immToken.value, ir.opcode, desc.imm_idx);
    }

    auto imm = static_cast<uint16_t>(static_cast<int16_t>(immVal));

    return (static_cast<uint32_t>(op.value) << 26)
         | (rs_val << 21)
         | (rt_val << 16)
         | imm;
}

Binary Assembler::assemble(const std::vector<IR> &IR_INPUT)
{
    Binary bin;

    // ── Pass 1 : data segment ─────────────────────────────────────────────────
    // Walk the whole IR looking for the .data section.
    // Each  label: .word v1, v2, ...  entry is emitted into bin.dataBin
    // (little-endian) and the label's absolute address is recorded.
    //
    // Memory layout:
    //   bin.bin     → text segment → 0x00400000
    //   bin.dataBin → data segment → 0x10010000
    static constexpr uint32_t DATA_BASE = 0x10010000u;

    std::unordered_map<std::string, int32_t> dataLabelMap;
    {
        enum class Seg { NONE, TEXT, DATA } seg = Seg::NONE;
        std::string pendingLabel;
        uint32_t dataOffset = 0;

        for (const auto &node : IR_INPUT)
        {
            std::visit([&](auto &&n)
            {
                using T = std::decay_t<decltype(n)>;

                if constexpr (std::is_same_v<T, Directive>)
                {
                    if (n.name == ".data") { seg = Seg::DATA;  pendingLabel.clear(); return; }
                    if (n.name == ".text") { seg = Seg::TEXT;  pendingLabel.clear(); return; }

                    if (seg == Seg::DATA && n.name == ".word")
                    {
                        // --- 1. FORCE 4-BYTE ALIGNMENT ---
                        while (dataOffset % 4 != 0) {
                            bin.dataBin.push_back(0x00);
                            dataOffset++;
                        }

                        // --- 2. NOW MAP THE LABEL ---
                        if (!pendingLabel.empty())
                        {
                            dataLabelMap[pendingLabel] =
                                static_cast<int32_t>(DATA_BASE + dataOffset);
                            pendingLabel.clear();
                        }

                        // --- 3. WRITE THE WORDS ---
                        for (const auto &arg : n.args)
                        {
                            uint32_t w = static_cast<uint32_t>(
                                parseImm(arg.value, ".word", 0));
                            bin.dataBin.push_back( w        & 0xFF);
                            bin.dataBin.push_back((w >>  8) & 0xFF);
                            bin.dataBin.push_back((w >> 16) & 0xFF);
                            bin.dataBin.push_back((w >> 24) & 0xFF);
                            dataOffset += 4;
                        }
                    }
                    else if (seg == Seg::DATA && n.name == ".space")
                    {
                        //force alignment to 4 bytes
                        while (dataOffset % 4 != 0) {
                            bin.dataBin.push_back(0x00);
                            dataOffset++;
                        }

                        if (!pendingLabel.empty())
                        {
                            dataLabelMap[pendingLabel] =
                                static_cast<int32_t>(DATA_BASE + dataOffset);
                            pendingLabel.clear();
                        }

                        if (!n.args.empty())
                        {
                            uint32_t bytesToAlloc = static_cast<uint32_t>(
                                parseImm(n.args[0].value, ".space", 0));
                                
                            for (uint32_t i = 0; i < bytesToAlloc; ++i)
                            {
                                bin.dataBin.push_back(0x00);
                                dataOffset++;
                            }
                        }
                    }

                    if (seg == Seg::DATA &&
                        (n.name == ".asciiz" || n.name == ".ascii"))
                    {
                        if (!pendingLabel.empty())
                        {
                            dataLabelMap[pendingLabel] =
                                static_cast<int32_t>(DATA_BASE + dataOffset);
                            pendingLabel.clear();
                        }
                        for (const auto &arg : n.args)
                        {
                            for (unsigned char c : arg.value)
                            {
                                bin.dataBin.push_back(c);
                                dataOffset++;
                            }
                            if (n.name == ".asciiz")
                            {
                                bin.dataBin.push_back(0);  // null terminator
                                dataOffset++;
                            }
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, Label>)
                {
                    if (seg == Seg::DATA)
                        pendingLabel = n.name;
                }
            }, node);
        }
    }

    // ── Pass 2 : locate 'main' ────────────────────────────────────────────────
    size_t main_idx = IR_INPUT.size();

    for (size_t i = 0; i < IR_INPUT.size(); ++i)
    {
        bool found = std::visit([](auto &&node) -> bool
        {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, Label>)
                return node.name == "main";
            return false;
        }, IR_INPUT[i]);

        if (found) { main_idx = i; break; }
    }

    if (main_idx == IR_INPUT.size())
        throw std::runtime_error("Assembler: 'main' label not found in IR");

// ── Pass 3 : build text-segment label map ─────────────────────────────────
    // Text labels are stored as ABSOLUTE addresses (TEXT_BASE + byte_offset).
    // Data labels (already absolute) are merged in so all symbols resolve uniformly.
    static constexpr uint32_t TEXT_BASE = 0x00400000u;
    std::unordered_map<std::string, int32_t> labelMap = dataLabelMap;
    {
        enum class Seg { NONE, TEXT, DATA } seg = Seg::TEXT; // assume text initially after main
        int32_t offset = 0;
        
        for (size_t i = main_idx + 1; i < IR_INPUT.size(); ++i)
        {
            std::visit([&](auto &&node)
            {
                using T = std::decay_t<decltype(node)>;
                
                if constexpr (std::is_same_v<T, Directive>)
                {
                    // Track which segment we are currently in
                    if (node.name == ".data") seg = Seg::DATA;
                    else if (node.name == ".text") seg = Seg::TEXT;
                }
                else if constexpr (std::is_same_v<T, Label>)
                {
                    // ONLY overwrite if it's a text label. Data labels keep their Pass 1 address.
                    if (seg == Seg::TEXT)
                        labelMap[node.name] = static_cast<int32_t>(TEXT_BASE) + offset;
                }
                else if constexpr (std::is_same_v<T, Instruction>)
                {
                    if (seg == Seg::TEXT)
                        offset += (node.opcode == "la") ? 8 : 4;  // la expands to 2 words
                }
            }, IR_INPUT[i]);
        }
    }

    // ── Pass 4 : emit text instructions ──────────────────────────────────────
    int32_t currentAbsAddr = static_cast<int32_t>(TEXT_BASE);
    enum class Seg { NONE, TEXT, DATA } seg = Seg::TEXT;
    
    for (size_t i = main_idx + 1; i < IR_INPUT.size(); ++i)
    {
        std::visit([&](auto &&ir)
        {
            using T = std::decay_t<decltype(ir)>;

            if constexpr (std::is_same_v<T, Directive>)
            {
                if (ir.name == ".data") seg = Seg::DATA;
                else if (ir.name == ".text") seg = Seg::TEXT;
            }
            else if constexpr (std::is_same_v<T, Instruction>)
            {
                // Only encode instructions if we are actively in the text segment
                if (seg != Seg::TEXT) return;

                if (ir.opcode == "la")
                {
                    // Pseudo: la $rd, label  ->  lui $rd, upper16  +  ori $rd, $rd, lower16
                    uint32_t rd = TokenToBinary(ir.operands[0].value).value;
                    const Token &tok = ir.operands[1];
                    int32_t addr = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        auto it = labelMap.find(tok.value);
                        if (it == labelMap.end())
                            throw std::runtime_error("'la': undefined label '" + tok.value + "'");
                        addr = it->second;
                    }
                    else
                    {
                        addr = parseImm(tok.value, "la", 1);
                    }
                    uint32_t upper = (static_cast<uint32_t>(addr) >> 16) & 0xFFFF;
                    uint32_t lower =  static_cast<uint32_t>(addr)        & 0xFFFF;
                    
                    pushWord(bin, (0x0Fu << 26) | (rd << 16) | upper);              // lui
                    pushWord(bin, (0x0Du << 26) | (rd << 21) | (rd << 16) | lower); // ori

                    // PATCH: Map both the lui and ori instruction to the same line number 
                    // so the UI debugger highlight doesn't skip a beat
                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    PCtoLine[currentAbsAddr + 4] = static_cast<uint32_t>(ir.line); 

                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr + 4);
                    
                    currentAbsAddr += 8;
                }
                else if (ir.opcode == "j" || ir.opcode == "jal")
                {
                    // J-type: 26-bit word address of jump target
                    const Token &tok = ir.operands[0];
                    uint32_t absAddr = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        auto it = labelMap.find(tok.value);
                        if (it == labelMap.end())
                            throw std::runtime_error("'" + ir.opcode + "': undefined label '" + tok.value + "'");
                        absAddr = static_cast<uint32_t>(it->second);
                    }
                    else
                    {
                        absAddr = static_cast<uint32_t>(parseImm(tok.value, ir.opcode, 0));
                    }
                    uint32_t opcode = (ir.opcode == "jal") ? 0x03u : 0x02u;
                    pushWord(bin, (opcode << 26) | ((absAddr >> 2) & 0x3FFFFFFu));
                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else
                {
                    auto it = InstrFormatMap.find(ir.opcode);
                    if (it == InstrFormatMap.end())
                        throw std::runtime_error("Unknown opcode: " + ir.opcode);

                    const InstrDesc &desc = it->second;

                    if (desc.type == InstrType::I_TYPE)
                        pushWord(bin, encodeIType(ir, desc, labelMap, currentAbsAddr));
                    else if (desc.type == InstrType::R_TYPE)
                        pushWord(bin, encodeRType(ir, desc));

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
            }
        },
        IR_INPUT[i]);
    }

    return bin;
}

