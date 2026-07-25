#pragma once
#include <string>
#include <vector>

struct PipelineRow {
    uint32_t pc = 0;
    std::string lineStr; 
    std::string instructionText; // Cached standard string
    int startCycle = 0;
    std::vector<std::string> stages; // e.g. ["IF", "ID", "STALL", "EX", "MEM", "WB"]
};