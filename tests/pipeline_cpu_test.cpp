#include <cstdlib>
#include <iostream>
#include <string>

#include "memory.h"
#include "pipeline_cpu.h"

namespace {

constexpr uint32_t kHalt = 0x0FF00513U;

uint32_t encode_r(uint32_t funct7, uint32_t rs2, uint32_t rs1,
                  uint32_t funct3, uint32_t rd, uint32_t opcode = 0x33U) {
    return ((funct7 & 0x7FU) << 25U) |
           ((rs2 & 0x1FU) << 20U) |
           ((rs1 & 0x1FU) << 15U) |
           ((funct3 & 0x7U) << 12U) |
           ((rd & 0x1FU) << 7U) |
           (opcode & 0x7FU);
}

uint32_t encode_i(int32_t imm, uint32_t rs1, uint32_t funct3,
                  uint32_t rd, uint32_t opcode) {
    return ((static_cast<uint32_t>(imm) & 0xFFFU) << 20U) |
           ((rs1 & 0x1FU) << 15U) |
           ((funct3 & 0x7U) << 12U) |
           ((rd & 0x1FU) << 7U) |
           (opcode & 0x7FU);
}

uint32_t encode_s(int32_t imm, uint32_t rs2, uint32_t rs1,
                  uint32_t funct3, uint32_t opcode = 0x23U) {
    const uint32_t bits = static_cast<uint32_t>(imm) & 0xFFFU;
    return (((bits >> 5U) & 0x7FU) << 25U) |
           ((rs2 & 0x1FU) << 20U) |
           ((rs1 & 0x1FU) << 15U) |
           ((funct3 & 0x7U) << 12U) |
           ((bits & 0x1FU) << 7U) |
           (opcode & 0x7FU);
}

uint32_t encode_b(int32_t offset, uint32_t rs2, uint32_t rs1,
                  uint32_t funct3, uint32_t opcode = 0x63U) {
    const uint32_t bits = static_cast<uint32_t>(offset) & 0x1FFFU;
    return (((bits >> 12U) & 0x1U) << 31U) |
           (((bits >> 5U) & 0x3FU) << 25U) |
           ((rs2 & 0x1FU) << 20U) |
           ((rs1 & 0x1FU) << 15U) |
           ((funct3 & 0x7U) << 12U) |
           (((bits >> 1U) & 0xFU) << 8U) |
           (((bits >> 11U) & 0x1U) << 7U) |
           (opcode & 0x7FU);
}

uint32_t encode_j(int32_t offset, uint32_t rd, uint32_t opcode = 0x6FU) {
    const uint32_t bits = static_cast<uint32_t>(offset) & 0x1FFFFFU;
    return (((bits >> 20U) & 0x1U) << 31U) |
           (((bits >> 1U) & 0x3FFU) << 21U) |
           (((bits >> 11U) & 0x1U) << 20U) |
           (((bits >> 12U) & 0xFFU) << 12U) |
           ((rd & 0x1FU) << 7U) |
           (opcode & 0x7FU);
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void run_cpu(PipelineCpu& cpu, uint64_t cycle_limit = 1000U) {
    for (uint64_t i = 0U; i < cycle_limit; ++i) {
        const PipelineStepResult result = cpu.step_cycle();
        expect(result != PipelineStepResult::InvalidInstruction,
               "遇到了无效指令");
        if (result == PipelineStepResult::Halted) return;
    }

    expect(false, "超过最大周期数，CPU 可能没有正确停止");
}

void test_basic_forwarding() {
    Memory memory;

    // addi x1, x0, 10
    memory.write_word(0U, encode_i(10, 0U, 0U, 1U, 0x13U));
    // addi x2, x1, 5
    memory.write_word(4U, encode_i(5, 1U, 0U, 2U, 0x13U));
    // add x10, x2, x1
    memory.write_word(8U, encode_r(0U, 1U, 2U, 0U, 10U));
    memory.write_word(12U, kHalt);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(1U) == 10U, "x1 应为 10");
    expect(cpu.register_value(2U) == 15U, "x2 应为 15");
    expect(cpu.register_value(10U) == 25U, "双来源转发结果应为 25");
}

void test_load_use_and_memory_delay() {
    Memory memory;

    // addi x1, x0, 100
    memory.write_word(0U, encode_i(100, 0U, 0U, 1U, 0x13U));
    // lw x5, 0(x1)
    memory.write_word(4U, encode_i(0, 1U, 2U, 5U, 0x03U));
    // add x10, x5, x5
    memory.write_word(8U, encode_r(0U, 5U, 5U, 0U, 10U));
    memory.write_word(12U, kHalt);
    memory.write_word(100U, 21U);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(5U) == 21U, "lw 应读出 21");
    expect(cpu.register_value(10U) == 42U, "load-use 转发结果应为 42");
    expect(cpu.cycle_count() == 10U, "该程序在三周期 MEM 下应运行 10 cycle");
}

void test_frozen_operand_capture() {
    Memory memory;

    // addi x1, x0, 10
    memory.write_word(0U, encode_i(10, 0U, 0U, 1U, 0x13U));
    // lw x5, 100(x0)，用于占用 MEM 三个周期
    memory.write_word(4U, encode_i(100, 0U, 2U, 5U, 0x03U));
    // add x10, x1, x0，在 ID/EX 中冻结时需要保存 x1 的新值
    memory.write_word(8U, encode_r(0U, 0U, 1U, 0U, 10U));
    memory.write_word(12U, kHalt);
    memory.write_word(100U, 7U);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(5U) == 7U, "lw 应读出 7");
    expect(cpu.register_value(10U) == 10U,
           "被冻结指令应保存 MEM/WB 转发来的 x1=10");
}

void test_store_forwarding_and_order() {
    Memory memory;

    // addi x1, x0, 100
    memory.write_word(0U, encode_i(100, 0U, 0U, 1U, 0x13U));
    // addi x2, x0, 42
    memory.write_word(4U, encode_i(42, 0U, 0U, 2U, 0x13U));
    // sw x2, 0(x1)
    memory.write_word(8U, encode_s(0, 2U, 1U, 2U));
    // lw x10, 0(x1)
    memory.write_word(12U, encode_i(0, 1U, 2U, 10U, 0x03U));
    memory.write_word(16U, kHalt);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(memory.read_word(100U) == 42U, "sw 应写入 42");
    expect(cpu.register_value(10U) == 42U,
           "后续 lw 应读到前面 sw 写入的新值");
}

void test_branch_flush() {
    Memory memory;

    // addi x1, x0, 1
    memory.write_word(0U, encode_i(1, 0U, 0U, 1U, 0x13U));
    // beq x1, x1, 8，跳到地址 12
    memory.write_word(4U, encode_b(8, 1U, 1U, 0U));
    // 错误路径：addi x10, x0, 99
    memory.write_word(8U, encode_i(99, 0U, 0U, 10U, 0x13U));
    // 正确目标：addi x10, x0, 42
    memory.write_word(12U, encode_i(42, 0U, 0U, 10U, 0x13U));
    memory.write_word(16U, kHalt);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(10U) == 42U,
           "成立分支应清除错误路径指令");
}

void test_wrong_path_halt() {
    Memory memory;

    // beq x0, x0, 8，跳过地址 4 的 halt
    memory.write_word(0U, encode_b(8, 0U, 0U, 0U));
    memory.write_word(4U, kHalt);
    // addi x10, x0, 42
    memory.write_word(8U, encode_i(42, 0U, 0U, 10U, 0x13U));
    memory.write_word(12U, kHalt);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(10U) == 42U,
           "错误路径上的 halt 不应真正停止 CPU");
}

void test_jal_and_x0() {
    Memory memory;

    // jal x1, 8，x1 应得到 4，并跳到地址 8
    memory.write_word(0U, encode_j(8, 1U));
    // 错误路径：addi x10, x0, 99
    memory.write_word(4U, encode_i(99, 0U, 0U, 10U, 0x13U));
    // addi x0, x0, 123，x0 仍必须为 0
    memory.write_word(8U, encode_i(123, 0U, 0U, 0U, 0x13U));
    // addi x10, x0, 7
    memory.write_word(12U, encode_i(7, 0U, 0U, 10U, 0x13U));
    memory.write_word(16U, kHalt);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(1U) == 4U, "jal 应把 PC+4 写入 x1");
    expect(cpu.register_value(0U) == 0U, "x0 必须始终为 0");
    expect(cpu.register_value(10U) == 7U, "跳转目标程序结果应为 7");
}

void test_signed_and_unsigned_loads() {
    Memory memory;

    // lb x5, 100(x0)
    memory.write_word(0U, encode_i(100, 0U, 0U, 5U, 0x03U));
    // lbu x6, 100(x0)
    memory.write_word(4U, encode_i(100, 0U, 4U, 6U, 0x03U));
    // lh x7, 102(x0)
    memory.write_word(8U, encode_i(102, 0U, 1U, 7U, 0x03U));
    // lhu x10, 102(x0)
    memory.write_word(12U, encode_i(102, 0U, 5U, 10U, 0x03U));
    memory.write_word(16U, kHalt);

    memory.write_byte(100U, 0x80U);
    memory.write_half(102U, 0x8001U);

    PipelineCpu cpu(memory);
    run_cpu(cpu);

    expect(cpu.register_value(5U) == 0xFFFFFF80U, "lb 应进行 8 位符号扩展");
    expect(cpu.register_value(6U) == 0x00000080U, "lbu 应进行零扩展");
    expect(cpu.register_value(7U) == 0xFFFF8001U, "lh 应进行 16 位符号扩展");
    expect(cpu.register_value(10U) == 0x00008001U, "lhu 应进行零扩展");
}

} // namespace

int main() {
    test_basic_forwarding();
    test_load_use_and_memory_delay();
    test_frozen_operand_capture();
    test_store_forwarding_and_order();
    test_branch_flush();
    test_wrong_path_halt();
    test_jal_and_x0();
    test_signed_and_unsigned_loads();

    std::cout << "All PipelineCpu tests passed.\n";
    return 0;
}