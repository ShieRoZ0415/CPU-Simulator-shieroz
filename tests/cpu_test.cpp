#include <cassert>
#include <cstdint>

#include "../src/cpu.h"
#include "memory.h"

void test_addi_and_halt() {
    Memory memory;

    memory.write_word(0U, 0x02A00513U);
    memory.write_word(4U, 0x0FF00513U);

    Cpu cpu(memory);

    assert(cpu.pc() == 0U);
    assert(cpu.register_value(10U) == 0U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.pc() == 4U);
    assert(cpu.register_value(10U) == 42U);

    assert(cpu.step() == StepResult::Halted);
    assert(cpu.pc() == 4U);
    assert(cpu.register_value(10U) == 42U);
}

void test_x0_cannot_be_written() {
    Memory memory;

    memory.write_word(0U, 0x06400013U);
    memory.write_word(4U, 0x00700513U);
    memory.write_word(8U, 0x0FF00513U);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(0U) == 0U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(10U) == 7U);
}

void test_register_addition() {
    Memory memory;

    memory.write_word(0U, 0x00500093U);
    memory.write_word(4U, 0x00700113U);
    memory.write_word(8U, 0x00208533U);
    memory.write_word(12U, 0x0FF00513U);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(1U) == 5U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(2U) == 7U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(10U) == 12U);
}

void test_load_byte_sign_extension() {
    Memory memory;

    memory.write_word(0U, 0x06400093U);
    memory.write_word(4U, 0x00008503U);
    memory.write_word(8U, 0x0FF00513U);
    memory.write_byte(100U, 0xFFU);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(1U) == 100U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(10U) == 0xFFFFFFFFU);
}

void test_load_byte_zero_extension() {
    Memory memory;

    memory.write_word(0U, 0x06400093U);
    memory.write_word(4U, 0x0000C503U);
    memory.write_word(8U, 0x0FF00513U);
    memory.write_byte(100U, 0xFFU);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(10U) == 0x000000FFU);
}

void test_store_and_load_word() {
    Memory memory;

    memory.write_word(0U, 0x06400093U);
    memory.write_word(4U, 0x02A00113U);
    memory.write_word(8U, 0x0020A023U);
    memory.write_word(12U, 0x0000A503U);
    memory.write_word(16U, 0x0FF00513U);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.step() == StepResult::Continue);
    assert(cpu.step() == StepResult::Continue);

    assert(memory.read_word(100U) == 42U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(10U) == 42U);
}

void test_branch() {
    Memory memory;

    memory.write_word(0U, 0x00500093U);
    memory.write_word(4U, 0x00500113U);
    memory.write_word(8U, 0x00208463U);
    memory.write_word(12U, 0x00100513U);
    memory.write_word(16U, 0x02A00513U);
    memory.write_word(20U, 0x0FF00513U);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.step() == StepResult::Continue);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.pc() == 16U);

    assert(cpu.step() == StepResult::Continue);
    assert(cpu.register_value(10U) == 42U);
}

void test_invalid_instruction() {
    Memory memory;

    memory.write_word(0U, 0xFFFFFFFFU);

    Cpu cpu(memory);

    assert(cpu.step() == StepResult::InvalidInstruction);
    assert(cpu.pc() == 0U);
}

int main() {
    test_addi_and_halt();
    test_x0_cannot_be_written();
    test_register_addition();
    test_load_byte_sign_extension();
    test_load_byte_zero_extension();
    test_store_and_load_word();
    test_branch();
    test_invalid_instruction();

    return 0;
}