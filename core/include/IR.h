#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>

#include "Tokens.h"


struct Instruction
{
    std::string opcode;
    std::vector<Token> operands;
    int line;
    
    std::optional<std::string> label;
};

struct Directive
{
    std::string name;
    std::vector<Token> args;
    int line;
    
    std::optional<std::string> label;
};

struct Label
{
    std::string name;
    int line;
};


using IR = std::variant<Instruction, Directive, Label>;
using Program = std::vector<IR>;