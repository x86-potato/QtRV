#include <Assembler.h>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>

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
    {"bltz",  {InstrType::I_TYPE, /*rs*/0, /*rt*/-1, /*imm*/1}},
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

static bool isPseudoInstruction(const std::string &mnemonic)
{
    static const std::unordered_map<std::string, bool> pseudoSet = {
        {"la", true}, {"li", true}, {"move", true},
        {"nop", true}, {"not", true}, {"neg", true}, {"b", true},
        {"beqz", true}, {"bnez", true},
        {"blt", true}, {"bgt", true}, {"ble", true}, {"bge", true},
        {"subi", true},
    };
    return pseudoSet.count(mnemonic) != 0;
}

// Load/store opcodes that support the MARS/SPIM-style "<op> $rt, label"
// pseudo form (no base register) in addition to their real "<op> $rt,
// imm($rs)" form.
static bool isLoadStoreOpcode(const std::string &opcode)
{
    static const std::unordered_map<std::string, bool> ops = {
        {"lw", true}, {"sw", true}, {"lb", true}, {"lbu", true},
        {"lh", true}, {"lhu", true}, {"sb", true}, {"sh", true},
    };
    return ops.count(opcode) != 0;
}

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

// A real addi's immediate is a signed 16-bit field. When a literal doesn't
// fit, reference assemblers (MARS included) auto-expand it into a
// lui+ori+add sequence via $at rather than silently truncating it -- so we
// do the same everywhere an "add/sub with a literal" pseudo-form is used.
static bool fitsSignedImm16(int32_t v) { return v >= -32768 && v <= 32767; }
static uint32_t addImmediateWordCount(int32_t imm) { return fitsSignedImm16(imm) ? 1u : 3u; }

// Emits "rd = rs + imm", expanding into lui/ori/add via $at when imm doesn't
// fit addi's 16-bit signed immediate. Returns the number of words emitted
// (1 or 3) so the caller can advance PCtoLine/currentAbsAddr accordingly.
static uint32_t emitAddImmediate(Binary &bin, uint32_t rd, uint32_t rs, int32_t imm)
{
    if (fitsSignedImm16(imm))
    {
        auto imm16 = static_cast<uint16_t>(static_cast<int16_t>(imm));
        pushWord(bin, (0x08u << 26) | (rs << 21) | (rd << 16) | imm16); // addi
        return 1;
    }

    constexpr uint32_t AT_REG = 1; // $at, the assembler-reserved scratch register
    uint32_t upper = (static_cast<uint32_t>(imm) >> 16) & 0xFFFF;
    uint32_t lower =  static_cast<uint32_t>(imm)        & 0xFFFF;
    pushWord(bin, (0x0Fu << 26) | (AT_REG << 16) | upper);                 // lui $at, upper
    pushWord(bin, (0x0Du << 26) | (AT_REG << 21) | (AT_REG << 16) | lower); // ori $at, $at, lower
    pushWord(bin, (rs << 21) | (AT_REG << 16) | (rd << 11) | 0x20u);       // add rd, rs, $at
    return 3;
}

// Every pseudo-instruction (and the load/store-from-label pseudo form)
// expands to a fixed number of real words for a GIVEN instruction, so
// Pass 3 (label address calculation) and Pass 4 (code emission) always
// agree on instruction sizes.
static uint32_t instructionWordCount(const Instruction &ir)
{
    // la $rd, offset($rs)  ->  addi $rd, $rs, offset (or lui+ori+add if
    // offset doesn't fit 16 bits). This is also how "la $rd, ($rs)" -- a
    // common "copy register" idiom -- is handled, since a zero offset addi
    // is just a register copy.
    // la $rd, label_or_imm  ->  lui+ori  (2 words)
    if (ir.opcode == "la")
    {
        if (ir.operands.size() != 3) return 2;
        int32_t offset = parseImm(ir.operands[1].value, "la", 1);
        return addImmediateWordCount(offset);
    }
    if (ir.opcode == "li") return 2;

    if (ir.opcode == "subi" && ir.operands.size() == 3)
    {
        int32_t imm = parseImm(ir.operands[2].value, "subi", 2);
        return addImmediateWordCount(-imm);
    }
    if ((ir.opcode == "add" || ir.opcode == "addu" || ir.opcode == "sub" || ir.opcode == "subu") &&
        ir.operands.size() == 3 && ir.operands[2].type == TokenType::NUMBER)
    {
        int32_t imm = parseImm(ir.operands[2].value, ir.opcode, 2);
        if (ir.opcode == "sub" || ir.opcode == "subu") imm = -imm;
        return addImmediateWordCount(imm);
    }

    // Two-register conditional branches expand to "slt $at, ...; beq/bne $at, $zero, label".
    if (ir.opcode == "blt" || ir.opcode == "bgt" || ir.opcode == "ble" || ir.opcode == "bge")
        return 2;

    // "<op> $rt, label" (bare label, no base register) expands to
    // "lui $at, ...; <op> $rt, ...($at)" -- 2 words instead of 1.
    if (isLoadStoreOpcode(ir.opcode) && ir.operands.size() == 2) return 2;

    return 1;
}

InstrClass classifyInstruction(const std::string &mnemonic)
{
    if (mnemonic == "j" || mnemonic == "jal")
        return InstrClass::J_TYPE;
    if (mnemonic == "mul")
        return InstrClass::R_TYPE; // real instruction, but not in InstrFormatMap (see Pass 4)
    if (isPseudoInstruction(mnemonic))
        return InstrClass::PSEUDO;

    auto it = InstrFormatMap.find(mnemonic);
    if (it == InstrFormatMap.end())
        return InstrClass::UNKNOWN;

    switch (it->second.type) {
        case InstrType::R_TYPE: return InstrClass::R_TYPE;
        case InstrType::I_TYPE: return InstrClass::I_TYPE;
        case InstrType::J_TYPE: return InstrClass::J_TYPE;
    }
    return InstrClass::UNKNOWN;
}

// ─── Multi-file scoping (global vs. local labels) ──────────────────────────
//
// A label declared via ".globl NAME" (or ".global NAME") in a file is
// visible to every file -- it lives under its plain name in the shared
// symbol table. Any other label is local to the file that defined it: the
// same name can be reused as a local label in a different file without
// conflict, and a bare reference to a name always prefers a local label in
// the referencing file over a same-named global symbol (local scope shadows
// global, the same way a C translation unit's static symbols shadow an
// extern of the same name).

static size_t fileIndexForLine(const std::vector<AssemblerFileSpan> &spans, uint32_t line)
{
    for (size_t i = 0; i < spans.size(); ++i)
    {
        uint32_t start = spans[i].startLine;
        uint32_t end   = start + spans[i].lineCount; // exclusive
        if (line >= start && line < end)
            return i;
    }
    return 0; // no (or no matching) file spans given: treat everything as one file
}

static std::string scopedKey(const std::string &name, size_t fileIndex)
{
    return std::to_string(fileIndex) + "::" + name;
}

// The key a label definition should be stored under: its plain name if this
// file declared it global via ".globl", otherwise a key scoped to this file.
static std::string labelDefKey(const std::string &name, size_t fileIndex,
                                const std::vector<std::unordered_set<std::string>> &globalDeclByFile)
{
    if (fileIndex < globalDeclByFile.size() && globalDeclByFile[fileIndex].count(name))
        return name;
    return scopedKey(name, fileIndex);
}

// Resolves a label reference written at `referencingLine`: a local label in
// that same file takes precedence, falling back to the global symbol table.
static int32_t resolveLabel(const std::unordered_map<std::string, int32_t> &labelMap,
                             const std::vector<AssemblerFileSpan> &fileSpans,
                             const std::string &name, uint32_t referencingLine,
                             const std::string &opcodeForError)
{
    size_t fileIdx = fileIndexForLine(fileSpans, referencingLine);

    auto localIt = labelMap.find(scopedKey(name, fileIdx));
    if (localIt != labelMap.end())
        return localIt->second;

    auto globalIt = labelMap.find(name);
    if (globalIt != labelMap.end())
        return globalIt->second;

    throw std::runtime_error("'" + opcodeForError + "': undefined label '" + name
        + "' at line " + std::to_string(referencingLine));
}

// Scans the whole program for ".globl"/".global" directives up front, so
// Pass 1/3 know -- before they record any label -- which names each file
// exports.
static std::vector<std::unordered_set<std::string>> collectGlobalDeclarations(
    const std::vector<IR> &IR_INPUT, const std::vector<AssemblerFileSpan> &fileSpans)
{
    std::vector<std::unordered_set<std::string>> declByFile(std::max<size_t>(fileSpans.size(), 1));

    for (const auto &node : IR_INPUT)
    {
        std::visit([&](auto &&n)
        {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, Directive>)
            {
                if (n.name == ".globl" || n.name == ".global")
                {
                    size_t fileIdx = fileIndexForLine(fileSpans, static_cast<uint32_t>(n.line));
                    for (const auto &arg : n.args)
                        declByFile[fileIdx].insert(arg.value);
                }
            }
        }, node);
    }

    return declByFile;
}

// Bounds-checked operand access: a malformed instruction (too few operands
// for its mnemonic) now produces a clear assembler error instead of an
// out-of-bounds vector access.
static const Token &operandAt(const Instruction &ir, int idx)
{
    if (idx < 0 || static_cast<size_t>(idx) >= ir.operands.size())
        throw std::runtime_error("'" + ir.opcode + "' at line " + std::to_string(ir.line)
            + ": expected at least " + std::to_string(idx + 1)
            + " operand(s), got " + std::to_string(ir.operands.size()));
    return ir.operands[idx];
}

static uint32_t encodeRType(const Instruction &ir, const InstrDesc &desc)
{
    uint32_t rs    = 0;
    if (desc.rs_idx >= 0)
        rs = TokenToBinary(operandAt(ir, desc.rs_idx).value).value;

    uint32_t rt    = 0;
    if (desc.rt_idx >= 0)
        rt = TokenToBinary(operandAt(ir, desc.rt_idx).value).value;

    uint32_t rd    = 0;
    if (desc.rd_idx >= 0)
        rd = TokenToBinary(operandAt(ir, desc.rd_idx).value).value;

    uint32_t shamt = 0;
    if (desc.shamt_idx >= 0)
        shamt = static_cast<uint32_t>(parseImm(operandAt(ir, desc.shamt_idx).value, ir.opcode, desc.shamt_idx)) & 0x1F;

    // opcode is always 0 for R-type
    return (rs << 21) | (rt << 16) | (rd << 11) | (shamt << 6) | desc.funct;
}

static uint32_t encodeIType(const Instruction &ir, const InstrDesc &desc,
                             const std::unordered_map<std::string, int32_t> &labelMap,
                             const std::vector<AssemblerFileSpan> &fileSpans,
                             int32_t instrAbsAddr)
{
    BinaryMap op = TokenToBinary(ir.opcode);

    uint32_t rs_val = 0;
    if (desc.rs_idx >= 0)
        rs_val = TokenToBinary(operandAt(ir, desc.rs_idx).value).value;

    uint32_t rt_val = 0;
    if (desc.rt_idx >= 0)
        rt_val = TokenToBinary(operandAt(ir, desc.rt_idx).value).value;

    // Resolve the immediate: raw integer or a label reference (branch target).
    int32_t immVal = 0;
    const Token &immToken = operandAt(ir, desc.imm_idx);
    if (immToken.type == TokenType::IDENTIFIER)
    {
        int32_t targetAddr = resolveLabel(labelMap, fileSpans, immToken.value,
                                           static_cast<uint32_t>(ir.line), ir.opcode);
        // MIPS branch offset = (target_abs - (instr_abs + 4)) / 4
        immVal = (targetAddr - (instrAbsAddr + 4)) / 4;
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

Binary Assembler::assemble(const std::vector<IR> &IR_INPUT,
                            const std::vector<AssemblerFileSpan> &fileSpans)
{
    Binary bin;

    // ── Pass 0 : collect ".globl"/".global" declarations per file ───────────
    // Needed before Pass 1 records any label, so it already knows which
    // names each file exports into the shared/global symbol table.
    std::vector<std::unordered_set<std::string>> globalDeclByFile =
        collectGlobalDeclarations(IR_INPUT, fileSpans);

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
    std::unordered_map<std::string, int> dataLabelDefLine; // for duplicate-label error messages
    {
        enum class Seg { NONE, TEXT, DATA } seg = Seg::NONE;
        std::string pendingLabel;
        int pendingLabelLine = 0;
        uint32_t dataOffset = 0;

        // Records `pendingLabel` (if any) at the current dataOffset, scoped
        // to whichever file `pendingLabelLine` falls in unless it was
        // declared ".globl" in that file.
        auto recordDataLabel = [&]()
        {
            if (pendingLabel.empty()) return;

            size_t fileIdx = fileIndexForLine(fileSpans, static_cast<uint32_t>(pendingLabelLine));
            std::string key = labelDefKey(pendingLabel, fileIdx, globalDeclByFile);

            if (dataLabelMap.count(key))
                throw std::runtime_error("Duplicate label '" + pendingLabel
                    + "': redefined at line " + std::to_string(pendingLabelLine)
                    + " (first defined at line " + std::to_string(dataLabelDefLine[key]) + ")");

            dataLabelMap[key] = static_cast<int32_t>(DATA_BASE + dataOffset);
            dataLabelDefLine[key] = pendingLabelLine;
            pendingLabel.clear();
        };

        for (const auto &node : IR_INPUT)
        {
            std::visit([&](auto &&n)
            {
                using T = std::decay_t<decltype(n)>;

              try {
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
                        recordDataLabel();

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

                        recordDataLabel();

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
                        recordDataLabel();
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
                    {
                        pendingLabel = n.name;
                        pendingLabelLine = n.line;
                    }
                }
              } catch (const std::runtime_error &e) {
                // Safety net: tag the error with this node's line if it
                // isn't already, so it's always attributable to a file/line.
                std::string msg = e.what();
                if (msg.find("line ") == std::string::npos)
                    msg += " at line " + std::to_string(n.line);
                throw std::runtime_error(msg);
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

    // Text is emitted starting from 'main' (so its own address is always
    // TEXT_BASE, matching where execution begins) and then wraps around to
    // whatever text precedes it in source order -- e.g. a helper file whose
    // name happens to sort before the file containing main. Without this,
    // any .text content positioned before 'main' would silently never be
    // assembled at all.
    std::vector<size_t> emissionOrder;
    emissionOrder.reserve(IR_INPUT.size());
    for (size_t i = main_idx; i < IR_INPUT.size(); ++i) emissionOrder.push_back(i);
    for (size_t i = 0; i < main_idx; ++i) emissionOrder.push_back(i);

// ── Pass 3 : build text-segment label map ─────────────────────────────────
    // Text labels are stored as ABSOLUTE addresses (TEXT_BASE + byte_offset).
    // Data labels (already absolute) are merged in so all symbols resolve uniformly.
    static constexpr uint32_t TEXT_BASE = 0x00400000u;
    std::unordered_map<std::string, int32_t> labelMap = dataLabelMap;
    std::unordered_map<std::string, int> textLabelDefLine; // for duplicate-label error messages
    {
        enum class Seg { NONE, TEXT, DATA } seg = Seg::TEXT; // 'main' is always in .text
        int32_t offset = 0;

        for (size_t idx : emissionOrder)
        {
            std::visit([&](auto &&node)
            {
                using T = std::decay_t<decltype(node)>;

              try {
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
                    {
                        size_t fileIdx = fileIndexForLine(fileSpans, static_cast<uint32_t>(node.line));
                        std::string key = labelDefKey(node.name, fileIdx, globalDeclByFile);

                        if (dataLabelDefLine.count(key))
                            throw std::runtime_error("Duplicate label '" + node.name
                                + "': already used as a data label (line " + std::to_string(dataLabelDefLine[key])
                                + "), redefined at line " + std::to_string(node.line));
                        if (textLabelDefLine.count(key))
                            throw std::runtime_error("Duplicate label '" + node.name
                                + "': redefined at line " + std::to_string(node.line)
                                + " (first defined at line " + std::to_string(textLabelDefLine[key]) + ")");

                        textLabelDefLine[key] = node.line;
                        labelMap[key] = static_cast<int32_t>(TEXT_BASE) + offset;
                    }
                }
                else if constexpr (std::is_same_v<T, Instruction>)
                {
                    if (seg == Seg::TEXT)
                        offset += static_cast<int32_t>(instructionWordCount(node)) * 4;
                }
              } catch (const std::runtime_error &e) {
                // Safety net: tag the error with this node's line if it
                // isn't already, so it's always attributable to a file/line.
                std::string msg = e.what();
                if (msg.find("line ") == std::string::npos)
                    msg += " at line " + std::to_string(node.line);
                throw std::runtime_error(msg);
              }
            }, IR_INPUT[idx]);
        }
    }

    // ── Pass 4 : emit text instructions ──────────────────────────────────────
    int32_t currentAbsAddr = static_cast<int32_t>(TEXT_BASE);
    enum class Seg { NONE, TEXT, DATA } seg = Seg::TEXT; // 'main' is always in .text

    for (size_t idx : emissionOrder)
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

              try {
                if (ir.opcode == "la" && ir.operands.size() == 3)
                {
                    // Pseudo: la $rd, offset($rs)  ->  addi $rd, $rs, offset
                    // (an "effective address" load, not a memory access; a
                    // zero offset makes this a plain register copy, e.g. the
                    // common "la $rd, ($rs)" idiom)
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    int32_t offset = parseImm(operandAt(ir, 1).value, "la", 1);
                    uint32_t rs = TokenToBinary(operandAt(ir, 2).value).value;

                    uint32_t words = emitAddImmediate(bin, rd, rs, offset);
                    for (uint32_t w = 0; w < words; ++w)
                        PCtoLine[currentAbsAddr + w * 4] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += words * 4;
                }
                else if (ir.opcode == "la")
                {
                    // Pseudo: la $rd, label_or_imm  ->  lui $rd, upper16  +  ori $rd, $rd, lower16
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    const Token &tok = operandAt(ir, 1);
                    int32_t addr = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        addr = resolveLabel(labelMap, fileSpans, tok.value,
                                             static_cast<uint32_t>(ir.line), "la");
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
                else if (ir.opcode == "li")
                {
                    // Pseudo: li $rd, imm  ->  lui $rd, upper16  +  ori $rd, $rd, lower16
                    // (always 2 words, matching Pass 3's fixed size for "li")
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    int32_t imm = parseImm(operandAt(ir, 1).value, "li", 1);

                    uint32_t upper = (static_cast<uint32_t>(imm) >> 16) & 0xFFFF;
                    uint32_t lower =  static_cast<uint32_t>(imm)        & 0xFFFF;

                    pushWord(bin, (0x0Fu << 26) | (rd << 16) | upper);              // lui
                    pushWord(bin, (0x0Du << 26) | (rd << 21) | (rd << 16) | lower); // ori

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    PCtoLine[currentAbsAddr + 4] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr + 4);

                    currentAbsAddr += 8;
                }
                else if (ir.opcode == "move")
                {
                    // Pseudo: move $rd, $rs  ->  addu $rd, $rs, $zero
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rs = TokenToBinary(operandAt(ir, 1).value).value;
                    pushWord(bin, (rs << 21) | (rd << 11) | 0x21u); // funct=addu, rt=$zero=0

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "nop")
                {
                    // Pseudo: nop  ->  sll $zero, $zero, 0  (the all-zero word)
                    pushWord(bin, 0);

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "not")
                {
                    // Pseudo: not $rd, $rs  ->  nor $rd, $rs, $zero
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rs = TokenToBinary(operandAt(ir, 1).value).value;
                    pushWord(bin, (rs << 21) | (rd << 11) | 0x27u); // funct=nor, rt=$zero=0

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "neg")
                {
                    // Pseudo: neg $rd, $rs  ->  sub $rd, $zero, $rs
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rt = TokenToBinary(operandAt(ir, 1).value).value;
                    pushWord(bin, (rt << 16) | (rd << 11) | 0x22u); // funct=sub, rs=$zero=0

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "subi")
                {
                    // Pseudo: subi $rd, $rs, imm  ->  addi $rd, $rs, -imm
                    // (auto-expanded via $at if -imm doesn't fit 16 bits)
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rs = TokenToBinary(operandAt(ir, 1).value).value;
                    int32_t imm = parseImm(operandAt(ir, 2).value, "subi", 2);

                    uint32_t words = emitAddImmediate(bin, rd, rs, -imm);
                    for (uint32_t w = 0; w < words; ++w)
                        PCtoLine[currentAbsAddr + w * 4] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += words * 4;
                }
                else if ((ir.opcode == "add" || ir.opcode == "addu" ||
                          ir.opcode == "sub" || ir.opcode == "subu") &&
                         ir.operands.size() == 3 && ir.operands[2].type == TokenType::NUMBER)
                {
                    // Generous form: real MIPS only allows a register in the
                    // third slot, but some teaching assemblers accept a
                    // literal there too -> add/sub $rd, $rs, imm  ->  addi $rd, $rs, (+/-)imm
                    // (auto-expanded via $at if the immediate doesn't fit 16 bits)
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rs = TokenToBinary(operandAt(ir, 1).value).value;
                    int32_t imm = parseImm(operandAt(ir, 2).value, ir.opcode, 2);
                    if (ir.opcode == "sub" || ir.opcode == "subu") imm = -imm;

                    uint32_t words = emitAddImmediate(bin, rd, rs, imm);
                    for (uint32_t w = 0; w < words; ++w)
                        PCtoLine[currentAbsAddr + w * 4] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += words * 4;
                }
                else if (ir.opcode == "mul")
                {
                    // Real MIPS32 instruction using the SPECIAL2 opcode
                    // (0x1C), not the plain R-type (0x00) InstrFormatMap
                    // path -- rd = low 32 bits of rs * rt, no hi/lo.
                    uint32_t rd = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rs = TokenToBinary(operandAt(ir, 1).value).value;
                    uint32_t rt = TokenToBinary(operandAt(ir, 2).value).value;
                    pushWord(bin, (0x1Cu << 26) | (rs << 21) | (rt << 16) | (rd << 11) | 0x02u);

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "b")
                {
                    // Pseudo: b label  ->  beq $zero, $zero, label
                    const Token &tok = operandAt(ir, 0);
                    int32_t immVal = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        int32_t targetAddr = resolveLabel(labelMap, fileSpans, tok.value,
                                                           static_cast<uint32_t>(ir.line), "b");
                        // MIPS branch offset = (target_abs - (instr_abs + 4)) / 4
                        immVal = (targetAddr - (currentAbsAddr + 4)) / 4;
                    }
                    else
                    {
                        immVal = parseImm(tok.value, "b", 0);
                    }
                    auto imm16 = static_cast<uint16_t>(static_cast<int16_t>(immVal));
                    pushWord(bin, (0x04u << 26) | imm16); // beq $zero, $zero, imm

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "beqz" || ir.opcode == "bnez")
                {
                    // Pseudo: beqz $rs, label  ->  beq $rs, $zero, label
                    //         bnez $rs, label  ->  bne $rs, $zero, label
                    uint32_t rs = TokenToBinary(operandAt(ir, 0).value).value;
                    const Token &tok = operandAt(ir, 1);
                    int32_t immVal = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        int32_t targetAddr = resolveLabel(labelMap, fileSpans, tok.value,
                                                           static_cast<uint32_t>(ir.line), ir.opcode);
                        immVal = (targetAddr - (currentAbsAddr + 4)) / 4;
                    }
                    else
                    {
                        immVal = parseImm(tok.value, ir.opcode, 1);
                    }
                    auto imm16 = static_cast<uint16_t>(static_cast<int16_t>(immVal));
                    uint32_t opcode = (ir.opcode == "beqz") ? 0x04u : 0x05u; // beq : bne
                    pushWord(bin, (opcode << 26) | (rs << 21) | imm16); // rt = $zero = 0

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
                else if (ir.opcode == "blt" || ir.opcode == "bgt" ||
                         ir.opcode == "ble" || ir.opcode == "bge")
                {
                    // Two-word expansions using $at as scratch:
                    //   blt $rs,$rt,label -> slt $at,$rs,$rt ; bne $at,$zero,label
                    //   bgt $rs,$rt,label -> slt $at,$rt,$rs ; bne $at,$zero,label
                    //   ble $rs,$rt,label -> slt $at,$rt,$rs ; beq $at,$zero,label
                    //   bge $rs,$rt,label -> slt $at,$rs,$rt ; beq $at,$zero,label
                    uint32_t rs = TokenToBinary(operandAt(ir, 0).value).value;
                    uint32_t rt = TokenToBinary(operandAt(ir, 1).value).value;
                    const Token &tok = operandAt(ir, 2);
                    constexpr uint32_t AT_REG = 1; // $at

                    bool swapOrder = (ir.opcode == "bgt" || ir.opcode == "ble");
                    uint32_t sltA = swapOrder ? rt : rs;
                    uint32_t sltB = swapOrder ? rs : rt;
                    pushWord(bin, (sltA << 21) | (sltB << 16) | (AT_REG << 11) | 0x2Au); // slt $at, sltA, sltB

                    int32_t immVal = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        int32_t targetAddr = resolveLabel(labelMap, fileSpans, tok.value,
                                                           static_cast<uint32_t>(ir.line), ir.opcode);
                        // The branch is the SECOND word, so its own address is currentAbsAddr + 4.
                        immVal = (targetAddr - (currentAbsAddr + 4 + 4)) / 4;
                    }
                    else
                    {
                        immVal = parseImm(tok.value, ir.opcode, 2);
                    }
                    auto imm16 = static_cast<uint16_t>(static_cast<int16_t>(immVal));

                    bool branchIfZero = (ir.opcode == "ble" || ir.opcode == "bge");
                    uint32_t branchOpcode = branchIfZero ? 0x04u : 0x05u; // beq : bne
                    pushWord(bin, (branchOpcode << 26) | (AT_REG << 21) | imm16); // rt = $zero = 0

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    PCtoLine[currentAbsAddr + 4] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr + 4);
                    currentAbsAddr += 8;
                }
                else if (ir.opcode == "j" || ir.opcode == "jal")
                {
                    // J-type: 26-bit word address of jump target
                    const Token &tok = operandAt(ir, 0);
                    uint32_t absAddr = 0;
                    if (tok.type == TokenType::IDENTIFIER)
                    {
                        absAddr = static_cast<uint32_t>(resolveLabel(labelMap, fileSpans, tok.value,
                                                                      static_cast<uint32_t>(ir.line), ir.opcode));
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
                else if (isLoadStoreOpcode(ir.opcode) && ir.operands.size() == 2 &&
                         ir.operands[1].type == TokenType::IDENTIFIER)
                {
                    // Pseudo: <op> $rt, label (no base register) ->
                    //   lui $at, upper16(addr) ; <op> $rt, lower16(addr)($at)
                    uint32_t rt = TokenToBinary(operandAt(ir, 0).value).value;
                    const Token &tok = operandAt(ir, 1);
                    int32_t addr = resolveLabel(labelMap, fileSpans, tok.value,
                                                 static_cast<uint32_t>(ir.line), ir.opcode);

                    uint32_t upper = (static_cast<uint32_t>(addr) >> 16) & 0xFFFF;
                    uint32_t lower =  static_cast<uint32_t>(addr)        & 0xFFFF;
                    // Unlike la/li (which use ori's zero-extended immediate for
                    // the low half), the real lw/sw here sign-extends its
                    // 16-bit immediate. If the low half's sign bit is set,
                    // that sign-extension would subtract 0x10000 from the
                    // effective address, so bump the upper half by 1 to
                    // compensate -- the standard lui/addi-style correction.
                    if (lower & 0x8000) upper = (upper + 1) & 0xFFFF;
                    constexpr uint32_t AT_REG = 1; // $at, the assembler-reserved scratch register

                    BinaryMap op = TokenToBinary(ir.opcode);

                    pushWord(bin, (0x0Fu << 26) | (AT_REG << 16) | upper); // lui $at, upper
                    pushWord(bin, (static_cast<uint32_t>(op.value) << 26)
                                | (AT_REG << 21) | (rt << 16) | lower);    // <op> $rt, lower($at)

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    PCtoLine[currentAbsAddr + 4] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr + 4);

                    currentAbsAddr += 8;
                }
                else
                {
                    auto it = InstrFormatMap.find(ir.opcode);
                    if (it == InstrFormatMap.end())
                        throw std::runtime_error("Unknown opcode: " + ir.opcode);

                    const InstrDesc &desc = it->second;

                    if (desc.type == InstrType::I_TYPE)
                        pushWord(bin, encodeIType(ir, desc, labelMap, fileSpans, currentAbsAddr));
                    else if (desc.type == InstrType::R_TYPE)
                        pushWord(bin, encodeRType(ir, desc));

                    PCtoLine[currentAbsAddr] = static_cast<uint32_t>(ir.line);
                    LinetoPC[ir.line] = static_cast<uint32_t>(currentAbsAddr);
                    currentAbsAddr += 4;
                }
              } catch (const std::runtime_error &e) {
                // Safety net: tag the error with this instruction's line if
                // whatever threw it (parseImm, TokenToBinary, etc.) didn't
                // already mention one, so it's always attributable to a
                // file/line by the GUI's error-message translation.
                std::string msg = e.what();
                if (msg.find("line ") == std::string::npos)
                    msg += " at line " + std::to_string(ir.line);
                throw std::runtime_error(msg);
              }
            }
        },
        IR_INPUT[idx]);
    }

    return bin;
}

