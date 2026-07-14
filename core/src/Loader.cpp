#include "Loader.h"

void Loader::load(const Binary& bin, Memory& mem,
                  uint32_t textBase, uint32_t dataBase)
{
    uint32_t addr = textBase;
    for (uint8_t byte : bin.bin)
        mem.writeByte(addr++, byte);

    addr = dataBase;
    for (uint8_t byte : bin.dataBin)
        mem.writeByte(addr++, byte);
}
