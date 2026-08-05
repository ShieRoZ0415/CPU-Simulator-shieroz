#include <iostream>
#include  "memory.h"
#include  "tomasulo_cpu.h"
#include  "loader.h"

int main() {
    Memory memory;
    if (!load_data(memory, std::cin)) return 1;
    TomasuloCpu cpu(memory);
    while (true) {
        TomasuloStepResult state = cpu.step_cycle();
        if (state == TomasuloStepResult::Halted) break;
        if (state == TomasuloStepResult::InvalidInstruction) return 1;

    }
    std::cout << (cpu.register_value(10U) & 0xFFU) << std::endl;
    return 0;
}
