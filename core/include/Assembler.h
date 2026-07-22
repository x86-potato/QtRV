#pragma once

#include <vector>
#include <variant>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <map>

#include "Tokens.h"
#include "IR.h"
#include "Binary.h"

struct BinaryMap
{
    size_t bit_length;
    size_t value;
};



const inline BinaryMap TokenToBinary(const std::string &token)
{
    static const std::unordered_map<std::string, BinaryMap> TokenToBinaryMap =
    {
        // I-type opcodes
        // Branches
        {"beq",   {6, 0x04}},
        {"bne",   {6, 0x05}},
        {"blez",  {6, 0x06}},
        {"bgtz",  {6, 0x07}},
        // Arithmetic / logic
        {"addi",  {6, 0x08}},
        {"addiu", {6, 0x09}},
        {"slti",  {6, 0x0A}},
        {"sltiu", {6, 0x0B}},
        {"andi",  {6, 0x0C}},
        {"ori",   {6, 0x0D}},
        {"xori",  {6, 0x0E}},
        {"lui",   {6, 0x0F}},
        // Loads
        {"lb",    {6, 0x20}},
        {"lh",    {6, 0x21}},
        {"lwl",   {6, 0x22}},
        {"lw",    {6, 0x23}},
        {"lbu",   {6, 0x24}},
        {"lhu",   {6, 0x25}},
        {"lwr",   {6, 0x26}},
        {"ll",    {6, 0x30}},
        // Stores
        {"sb",    {6, 0x28}},
        {"sh",    {6, 0x29}},
        {"swl",   {6, 0x2A}},
        {"sw",    {6, 0x2B}},
        {"swr",   {6, 0x2E}},
        {"sc",    {6, 0x38}},

        // MIPS registers
        {"$zero", {5,  0}},
        {"$at",   {5,  1}},
        {"$v0",   {5,  2}},
        {"$v1",   {5,  3}},
        {"$a0",   {5,  4}},
        {"$a1",   {5,  5}},
        {"$a2",   {5,  6}},
        {"$a3",   {5,  7}},
        {"$t0",   {5,  8}},
        {"$t1",   {5,  9}},
        {"$t2",   {5, 10}},
        {"$t3",   {5, 11}},
        {"$t4",   {5, 12}},
        {"$t5",   {5, 13}},
        {"$t6",   {5, 14}},
        {"$t7",   {5, 15}},
        {"$s0",   {5, 16}},
        {"$s1",   {5, 17}},
        {"$s2",   {5, 18}},
        {"$s3",   {5, 19}},
        {"$s4",   {5, 20}},
        {"$s5",   {5, 21}},
        {"$s6",   {5, 22}},
        {"$s7",   {5, 23}},
        {"$t8",   {5, 24}},
        {"$t9",   {5, 25}},
        {"$k0",   {5, 26}},
        {"$k1",   {5, 27}},
        {"$gp",   {5, 28}},
        {"$sp",   {5, 29}},
        {"$fp",   {5, 30}},
        {"$ra",   {5, 31}},
    };

    auto it = TokenToBinaryMap.find(token);
    if (it == TokenToBinaryMap.end())
        throw std::runtime_error("Unknown token in binary map: " + token);
    return it->second;

}


enum class InstrType { I_TYPE, R_TYPE, J_TYPE };

// Describes how to map assembly operands to machine-code fields.
// Indices refer to positions in Instruction::operands; -1 = implicit 0.
struct InstrDesc
{
    InstrType type;
    int       rs_idx    = -1;  // source register
    int       rt_idx    = -1;  // target register
    int       imm_idx   = -1;  // immediate (I-type)
    int       rd_idx    = -1;  // destination register (R-type)
    int       shamt_idx = -1;  // shift amount operand index (R-type), -1 = 0
    uint8_t   funct     =  0;  // function code (R-type)
};

class Assembler
{
public:
    std::map<uint32_t, uint32_t> PCtoLine; // Maps source line numbers to program counter addresses
    std::map<uint32_t, uint32_t> LinetoPC; // Maps program counter addresses to source line numbers

    Assembler() = default;

    Binary assemble(const std::vector<IR> &IR_INPUT);
};