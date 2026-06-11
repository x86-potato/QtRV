#include "MipsCPU.h"


using instruction32 = uint32_t;
void MipsCPU::tick()
{
    fetch();
    decode();
    execute();
    memoryAccess();
    writeBack();
}

void MipsCPU::fetch()
{
    // get instruction from memory at pc
    instruction32 fetched = 0; // TODO: read from memory




    pc += 4;

}

void MipsCPU::decode()
{

}

void MipsCPU::execute()
{

}

void MipsCPU::memoryAccess()
{

}

void MipsCPU::writeBack()
{

}