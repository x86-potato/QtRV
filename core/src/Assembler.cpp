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
};

static void pushWord(Binary &bin, uint32_t word)
{
    bin.bin.push_back((word >> 24) & 0xFF);
    bin.bin.push_back((word >> 16) & 0xFF);
    bin.bin.push_back((word >>  8) & 0xFF);
    bin.bin.push_back( word        & 0xFF);
}

Binary Assembler::assemble(const std::vector<IR> &IR_INPUT)
{
    Binary bin;

    for (const auto &ir_node : IR_INPUT)
    {
        std::visit([&bin](auto &&ir)
        {
            using T = std::decay_t<decltype(ir)>;

            if constexpr (std::is_same_v<T, Instruction>)
            {
                auto it = InstrFormatMap.find(ir.opcode);
                if (it == InstrFormatMap.end())
                    throw std::runtime_error("Unknown opcode: " + ir.opcode);

                const InstrDesc &desc = it->second;

                if (desc.type == InstrType::I_TYPE)
                {
                    std::string op_str = ir.opcode;
                    BinaryMap op = TokenToBinary(op_str);

                    uint32_t rs_val = 0;
                    if (desc.rs_idx >= 0)
                        rs_val = TokenToBinary(ir.operands[desc.rs_idx].value).value;

                    uint32_t rt_val = 0;
                    if (desc.rt_idx >= 0)
                        rt_val = TokenToBinary(ir.operands[desc.rt_idx].value).value;

                    auto imm = static_cast<uint16_t>(
                        static_cast<int16_t>(std::stoi(ir.operands[desc.imm_idx].value)));

                    uint32_t word = (static_cast<uint32_t>(op.value) << 26)
                                  | (rs_val << 21)
                                  | (rt_val << 16)
                                  | imm;

                    pushWord(bin, word);
                }
            }
            if constexpr (std::is_same_v<T, Directive>)
            {
            }
            if constexpr (std::is_same_v<T, Label>)
            {
            }
        },
        ir_node);
    }

    return bin;
}