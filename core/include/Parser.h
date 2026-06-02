#pragma once

#include <vector>
#include <stdexcept>

#include "Tokens.h"
#include "IR.h"

class Parser
{
public:
    Parser() = default;

    auto parse(const std::vector<Token>& tokens) -> Program;


};