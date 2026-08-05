#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "loader.h"
#include "memory.h"
#include "tomasulo_cpu.h"

struct TestResult {
    uint32_t value;
    uint64_t cycles;
};

TestResult run_one(const std::string& path, const int order[6]) {
    Memory memory;
    std::ifstream file(path);
    if (!file.is_open()) { std::cerr << "Failed open: " << path << std::endl; std::abort(); }
    if (!load_data(memory, file)) { std::cerr << "Failed load: " << path << std::endl; std::abort(); }
    TomasuloCpu cpu(memory);
    while (true) {
        TomasuloStepResult s = cpu.step_cycle_with_order(order);
        if (s == TomasuloStepResult::Halted) break;
        if (s == TomasuloStepResult::InvalidInstruction) {
            std::cerr << "Invalid instr: " << path << std::endl;
            std::abort();
        }
    }
    return {cpu.register_value(10U) & 0xFFU, cpu.cycle_count()};
}

int main() {
    const char* cases[] = {
        "array_test1", "array_test2", "basicopt1", "bulgarian",
        "expr", "gcd", "hanoi", "lvalue2", "magic",
        "manyarguments", "multiarray", "naive", "pi",
        "qsort", "queens", "statement_test", "superloop", "tak"
    };
    constexpr int N = sizeof(cases) / sizeof(cases[0]);
    const int ord_orig[6] = {0,1,2,3,4,5};
    const int ord_rev[6]  = {5,4,3,2,1,0};

    std::string base = "data/testcases/";
    int pass = 0, fail = 0;

    for (int i = 0; i < N; ++i) {
        std::string path = base + cases[i] + ".data";
        std::cout << "Testing " << cases[i] << " ... " << std::flush;
        TestResult r1 = run_one(path, ord_orig);
        TestResult r2 = run_one(path, ord_rev);
        if (r1.value == r2.value && r1.cycles == r2.cycles) {
            std::cout << "PASS (val=" << r1.value << ", cycles=" << r1.cycles << ")" << std::endl;
            ++pass;
        } else {
            std::cout << "FAIL" << std::endl;
            std::cout << "  original: val=" << r1.value << " cycles=" << r1.cycles << std::endl;
            std::cout << "  reversed: val=" << r2.value << " cycles=" << r2.cycles << std::endl;
            ++fail;
        }
    }

    std::cout << std::endl << pass << " passed, " << fail << " failed (" << N << " total)" << std::endl;
    assert(fail == 0);
    return 0;
}
