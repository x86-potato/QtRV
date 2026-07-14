#pragma once

#include <vector>

class Binary
{
public:
    std::vector<uint8_t> bin;     // text segment — instructions (little-endian words)
    std::vector<uint8_t> dataBin; // data segment — global variables (little-endian words)

    Binary()
    {
        bin.reserve(4096);
        dataBin.reserve(4096);
    }
};

