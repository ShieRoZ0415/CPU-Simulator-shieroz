#include <cassert>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <string>

#include "loader.h"
#include "memory.h"
#include "tomasulo_cpu.h"

int main() {
    struct TestCase {
        const char* name;
        uint32_t expected;
    };

    const TestCase cases[] = {
        {"array_test1",     123},
        {"array_test2",      43},
        {"basicopt1",        88},
        {"bulgarian",       159},
        {"expr",             58},
        {"gcd",             178},
        {"hanoi",            20},
        {"lvalue2",         175},
        {"magic",           106},
        {"manyarguments",    40},
        {"multiarray",      115},
        {"naive",            94},
        {"pi",              137},
        {"qsort",           105},
        {"queens",          171},
        {"statement_test",   50},
        {"superloop",       134},
        {"tak",             186},
    };

    constexpr int N = sizeof(cases) / sizeof(cases[0]);
    const std::string base = "../data/testcases/";
    int pass = 0, fail = 0;

    std::cout << std::left;
    std::cout << "+" << std::string(24, '-') << "+" << std::string(10, '-') << "+" << std::string(10, '-')
              << "+" << std::string(12, '-') << "+" << std::string(12, '-') << "+" << std::string(12, '-')
              << "+" << std::string(10, '-') << "+" << std::string(10, '-') << "+" << std::endl;
    std::cout << "| " << std::setw(22) << "Test" << " | " << std::setw(8) << "Expected"
              << " | " << std::setw(8) << "Actual" << " | " << std::setw(10) << "Result"
              << " | " << std::setw(10) << "Cycles" << " | " << std::setw(10) << "Br Total"
              << " | " << std::setw(8) << "Br Corr" << " | " << std::setw(8) << "Br Acc%" << " |" << std::endl;
    std::cout << "+" << std::string(24, '-') << "+" << std::string(10, '-') << "+" << std::string(10, '-')
              << "+" << std::string(12, '-') << "+" << std::string(12, '-') << "+" << std::string(12, '-')
              << "+" << std::string(10, '-') << "+" << std::string(10, '-') << "+" << std::endl;

    uint64_t total_predictions = 0;
    uint64_t total_correct = 0;

    for (int i = 0; i < N; ++i) {
        Memory memory;
        const std::string path = base + cases[i].name + ".data";
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open: " << path << std::endl;
            std::abort();
        }
        if (!load_data(memory, file)) {
            std::cerr << "Failed to load: " << path << std::endl;
            std::abort();
        }

        TomasuloCpu cpu(memory);
        bool ok = true;
        while (true) {
            TomasuloStepResult s = cpu.step_cycle();
            if (s == TomasuloStepResult::Halted) break;
            if (s == TomasuloStepResult::InvalidInstruction) {
                ok = false;
                break;
            }
        }

        uint32_t actual = cpu.register_value(10U) & 0xFFU;
        bool matched = ok && actual == cases[i].expected;
        if (matched) ++pass; else ++fail;

        uint64_t predictions = cpu.branch_predictions();
        uint64_t correct = cpu.correct_branch_predictions();
        total_predictions += predictions;
        total_correct += correct;

        double acc = cpu.branch_accuracy() * 100.0;

        std::cout << "| " << std::setw(22) << cases[i].name
                  << " | " << std::setw(8) << cases[i].expected
                  << " | " << std::setw(8) << actual
                  << " | " << std::setw(10);
        if (!ok)
            std::cout << "INVALID";
        else if (matched)
            std::cout << "PASS";
        else
            std::cout << "FAIL";
        std::cout << " | " << std::setw(10) << cpu.cycle_count()
                  << " | " << std::setw(10) << predictions
                  << " | " << std::setw(8) << correct
                  << " | " << std::setw(7) << std::fixed << std::setprecision(1) << acc << "% |" << std::endl;
    }

    std::cout << "+" << std::string(24, '-') << "+" << std::string(10, '-') << "+" << std::string(10, '-')
              << "+" << std::string(12, '-') << "+" << std::string(12, '-') << "+" << std::string(12, '-')
              << "+" << std::string(10, '-') << "+" << std::string(10, '-') << "+" << std::endl;

    double overall_acc = total_predictions > 0
                             ? static_cast<double>(total_correct) / static_cast<double>(total_predictions) * 100.0
                             : 0.0;
    std::cout << std::endl;
    std::cout << pass << " passed, " << fail << " failed (" << N << " total)" << std::endl;
    std::cout << "Overall branch prediction accuracy: " << std::fixed << std::setprecision(2)
              << overall_acc << "% (" << total_correct << "/" << total_predictions << ")" << std::endl;

    assert(fail == 0);
    return 0;
}
