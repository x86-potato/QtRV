#pragma once
#include <cstdint>

class Registers
{
    uint32_t zero = 0;

    uint32_t at = 0;

    uint32_t v0 = 0;
    uint32_t v1 = 0;

    uint32_t a0 = 0;
    uint32_t a1 = 0;
    uint32_t a2 = 0;
    uint32_t a3 = 0;

    uint32_t t0 = 0;
    uint32_t t1 = 0;
    uint32_t t2 = 0;
    uint32_t t3 = 0;
    uint32_t t4 = 0;
    uint32_t t5 = 0;
    uint32_t t6 = 0;
    uint32_t t7 = 0;    

    uint32_t s0 = 0;
    uint32_t s1 = 0;
    uint32_t s2 = 0;
    uint32_t s3 = 0;
    uint32_t s4 = 0;
    uint32_t s5 = 0;
    uint32_t s6 = 0;
    uint32_t s7 = 0;

    uint32_t t8 = 0;
    uint32_t t9 = 0;

    uint32_t k0 = 0;
    uint32_t k1 = 0;

    uint32_t gp = 0;
    uint32_t sp = 0;
    uint32_t fp = 0;
    uint32_t ra = 0;
};

class GP
{

};

class MipsCPU
{
public:
    uint32_t pc = 0;
    Registers regs;

    void tick();

    void fetch();
    void decode();
    void execute();
    void memoryAccess();
    void writeBack();

};