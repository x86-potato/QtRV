#include <Assembler.h>



Binary Assembler::assemble(const std::vector<IR> &IR_INPUT)
{
    Binary bin; 


    for (const auto &IR : IR_INPUT)
    {
        std::visit([](auto&& ir)
        {
            using T = std::decay_t<decltype(ir)>; 

            if constexpr (std::is_same_v<T,Instruction>)
            {
                
            }
            if constexpr (std::is_same_v<T, Directive>)
            {

            }
            if constexpr (std::is_same_v<T, Label>)
            {

            }
        },
        IR
        );


    }


    return bin;

}