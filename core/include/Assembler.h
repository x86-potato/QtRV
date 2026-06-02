#pragma once

#include <vector>
#include <variant>
#include "Tokens.h"
#include "IR.h"
#include "Binary.h"

class Assembler
{
public:
    Assembler() = default;

    Binary assemble(const std::vector<IR> &IR_INPUT);
};