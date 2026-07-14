#pragma once
#include <cstdint>
#include "Binary.h"
#include "Memory.h"

class Loader
{
public:
    // Writes text and data segments into memory at their respective base addresses.
    static void load(const Binary& bin, Memory& mem,
                     uint32_t textBase, uint32_t dataBase);
};
