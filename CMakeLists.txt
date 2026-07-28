cmake_minimum_required(VERSION 3.20)

project(RiscVSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(riscv_sim
        src/main.cpp
        src/memory.cpp
        src/decoder.cpp
)

target_include_directories(riscv_sim PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(memory_test
        tests/memory_test.cpp
        src/memory.cpp
)

target_include_directories(memory_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(decoder_test
        tests/decoder_test.cpp
        src/decoder.cpp
        src/instruction.h
)

target_include_directories(decoder_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)