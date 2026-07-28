#pragma once

#include <cstdint>

enum class InstructionFormat : uint8_t {
    Invalid,
    R,
    I,
    S,
    B,
    U,
    J
};

enum class Operation : uint8_t {
    Invalid,

    // U 型
    LUI,
    AUIPC,

    // 跳转
    JAL,
    JALR,

    // 条件分支
    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,

    // Load
    LB,
    LH,
    LW,
    LBU,
    LHU,

    // Store
    SB,
    SH,
    SW,

    // I 型
    ADDI,
    SLTI,
    SLTIU,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,

    // R 型
    ADD,
    SUB,
    SLL,
    SLT,
    SLTU,
    XOR,
    SRL,
    SRA,
    OR,
    AND
};

struct DecodedInstruction {
    uint32_t raw{0};
    Operation op{Operation::Invalid};
    InstructionFormat format{InstructionFormat::Invalid};
    // 默认都为 Invalid

    uint8_t rs1{0};
    uint8_t rs2{0};
    uint8_t rd{0};
    // 源寄存器和目的寄存器编号,取值范围为 0 到 31

    int32_t imm{0}; // 立即数

    // 是否真正读取 rs1, rs2
    bool uses_rs1{false};
    bool uses_rs2{false};

    // 当前指令是否写 rd
    bool writes_rd{false};

    bool valid() const noexcept{
        return op != Operation::Invalid;
    }
};

