cmake_minimum_required(VERSION 3.20)

project(RiscVSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(code
        src/main.cpp
        src/memory.cpp
        src/decoder.cpp
        src/cpu.cpp
        src/pipeline_cpu.h
        src/loader.cpp
        src/loader.h
        src/pipeline_cpu.cpp
        src/tomasulo_type.h
        src/tomasulo_cpu.cpp
        src/tomasulo_cpu.h
)

target_include_directories(code PRIVATE
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

add_executable(cpu_test
        tests/cpu_test.cpp
        src/cpu.cpp
        src/decoder.cpp
        src/memory.cpp
        src/cpu.h
        src/register_file.h
        src/pipeline_cpu.h
        src/pipeline_cpu.cpp
)
target_include_directories(cpu_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(pipeline_cpu_test
        tests/pipeline_cpu_test.cpp
        src/pipeline_cpu.cpp
        src/decoder.cpp
        src/memory.cpp
)
target_include_directories(pipeline_cpu_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

enable_testing()

add_test(NAME memory_test COMMAND memory_test)
add_test(NAME decoder_test COMMAND decoder_test)
add_test(NAME cpu_test COMMAND cpu_test)
add_test(NAME pipeline_cpu_test COMMAND pipeline_cpu_test)