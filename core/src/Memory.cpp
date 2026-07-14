#include "Memory.h"
#include <cstring>

Memory::Memory()
{
    // pages are allocated on demand— nothing to pre-allocate
}

static Page* getPage(std::unordered_map<Word, Page>& map, Word address, bool allocate)
{
    Word pageIndex = address >> 12;
    auto it = map.find(pageIndex);
    if (it != map.end())
        return &it->second;
    if (!allocate)
        return nullptr;
    Page& p = map[pageIndex];
    std::memset(p.data, 0, PAGE_SIZE);
    return &p;
}

uint8_t* Memory::readByte(Word memAddress)
{
    Page* p = getPage(m_memory, memAddress, false);
    if (!p) return nullptr;
    return &p->data[memAddress & 0xFFF];
}

bool Memory::writeByte(Word memAddress, uint8_t byteValue)
{
    Page* p = getPage(m_memory, memAddress, true);
    p->data[memAddress & 0xFFF] = byteValue;
    return true;
}

uint16_t* Memory::readHalfword(Word memAddress)
{
    if (memAddress & 0x1) return nullptr;
    if ((memAddress & 0xFFF) > PAGE_SIZE - 2) return nullptr;
    Page* p = getPage(m_memory, memAddress, false);
    if (!p) return nullptr;
    return reinterpret_cast<uint16_t*>(&p->data[memAddress & 0xFFF]);
}

bool Memory::writeHalfword(Word memAddress, uint16_t halfwordValue)
{
    Word offset = memAddress & 0xFFF;
    if (offset <= PAGE_SIZE - 2)
    {
        Page* p = getPage(m_memory, memAddress, true);
        std::memcpy(&p->data[offset], &halfwordValue, 2);
    }
    else
    {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(&halfwordValue);
        writeByte(memAddress,     src[0]);
        writeByte(memAddress + 1, src[1]);
    }
    return true;
}

uint32_t* Memory::readWord(Word memAddress)
{
    if (memAddress & 0x3) return nullptr;
    if ((memAddress & 0xFFF) > PAGE_SIZE - 4) return nullptr;
    Page* p = getPage(m_memory, memAddress, false);
    if (!p) return nullptr;
    return reinterpret_cast<uint32_t*>(&p->data[memAddress & 0xFFF]);
}

bool Memory::writeWord(Word memAddress, uint32_t wordValue)
{
    Word offset = memAddress & 0xFFF;
    if (offset <= PAGE_SIZE - 4)
    {
        Page* p = getPage(m_memory, memAddress, true);
        std::memcpy(&p->data[offset], &wordValue, 4);
    }
    else
    {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(&wordValue);
        for (int i = 0; i < 4; ++i)
            writeByte(memAddress + i, src[i]);
    }
    return true;
}
