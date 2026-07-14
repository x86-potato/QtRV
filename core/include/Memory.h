//here we will hold our memory, we will keep it vauge and modular as a "1d array"
//we will dynamically allocate pages as needed

//we will also contain the binary to be loaded into the fake "memory", the copy will
// be freed after use, so its not repeated


#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
   
static constexpr size_t PAGE_SIZE = 4096; // 4KB page size
static constexpr size_t STACK_SIZE = 1024 * 1024; // 1MB stack size
    

using Word = uint32_t; // 32-bit word

struct Page
{
    uint8_t data[PAGE_SIZE]; // 4KB page size
};

class Memory
{   
    std::unordered_map<Word, Page> m_memory; // address to page mapping
    std::vector<uint8_t> stack;
    //pages owned by memory, will be freed when memory is destroyed

public:
    Memory();


    uint8_t* readByte(Word memAddress);
    bool writeByte(Word memAddress, uint8_t byteValue);

    uint16_t* readHalfword(Word memAddress);
    bool writeHalfword(Word memAddress, uint16_t halfwordValue);

    uint32_t* readWord(Word memAddress);
    bool writeWord(Word memAddress, uint32_t wordValue);


};