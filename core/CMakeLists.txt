add_library(core STATIC
    src/Emulator.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Assembler.cpp
    src/Loader.cpp
    src/MipsCPU.cpp
    include/Buffer.h
    include/IR.h
    src/Memory.cpp
    include/InstructionState.h
)

target_include_directories(core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
