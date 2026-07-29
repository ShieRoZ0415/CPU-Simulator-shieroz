#include <iostream>
#include  "memory.h"
#include  "cpu.h"
#include  "loader.h"

int main() {
    Memory memory;
    if (!load_data(memory, std::cin)) return 1;
    Cpu cpu(memory);
    while (true) {
        StepResult state = cpu.step();
        if (state == StepResult::Halted) break;
        if (state == StepResult::InvalidInstruction) return 1;

    }
    std::cout << cpu.register_value(10U) & 0xFFU << std::endl;
    return 0;
}
