#pragma once

#include <vector>

class Binary
{


public:

    std::vector<uint8_t> bin;

    Binary() 
    {
        bin.reserve(4096);
    } 
};
