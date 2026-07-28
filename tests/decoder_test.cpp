#include "decoder.h"

#include <cassert>
#include <cstdint>
#include <iostream>

uint32_t encode_r(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return (funct7 << 25U) |
           (rs2 << 20U) |
           (rs1 << 15U) |
           (funct3 << 12U) |
           (rd << 7U) |
           opcode;
}

uint32_t encode_i(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    uint32_t imm_bits = static_cast<uint32_t>(imm) & 0xFFFU;

    return (imm_bits << 20U) |
           (rs1 << 15U) |
           (funct3 << 12U) |
           (rd << 7U) |
           opcode;
}

uint32_t encode_s(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    uint32_t imm_bits = static_cast<uint32_t>(imm) & 0xFFFU;
    uint32_t imm_11_5 = (imm_bits >> 5U) & 0x7FU;
    uint32_t imm_4_0 = imm_bits & 0x1FU;

    return (imm_11_5 << 25U) |
           (rs2 << 20U) |
           (rs1 << 15U) |
           (funct3 << 12U) |
           (imm_4_0 << 7U) |
           opcode;
}

uint32_t encode_b(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    uint32_t imm_bits = static_cast<uint32_t>(imm) & 0x1FFFU;

    return (((imm_bits >> 12U) & 0x1U) << 31U) |
           (((imm_bits >> 5U) & 0x3FU) << 25U) |
           (rs2 << 20U) |
           (rs1 << 15U) |
           (funct3 << 12U) |
           (((imm_bits >> 1U) & 0xFU) << 8U) |
           (((imm_bits >> 11U) & 0x1U) << 7U) |
           opcode;
}

uint32_t encode_u(uint32_t imm20, uint32_t rd, uint32_t opcode) {
    return ((imm20 & 0xFFFFFU) << 12U) |
           (rd << 7U) |
           opcode;
}

uint32_t encode_j(int32_t imm, uint32_t rd, uint32_t opcode) {
    uint32_t imm_bits = static_cast<uint32_t>(imm) & 0x1FFFFFU;

    return (((imm_bits >> 20U) & 0x1U) << 31U) |
           (((imm_bits >> 1U) & 0x3FFU) << 21U) |
           (((imm_bits >> 11U) & 0x1U) << 20U) |
           (((imm_bits >> 12U) & 0xFFU) << 12U) |
           (rd << 7U) |
           opcode;
}

void check_instruction(uint32_t raw, Operation op, InstructionFormat format, uint8_t rd, uint8_t rs1, uint8_t rs2, int32_t imm, bool uses_rs1, bool uses_rs2, bool writes_rd) {
    DecodedInstruction instruction = Decoder::decode(raw);

    assert(instruction.valid());
    assert(instruction.raw == raw);
    assert(instruction.op == op);
    assert(instruction.format == format);
    assert(instruction.rd == rd);
    assert(instruction.rs1 == rs1);
    assert(instruction.rs2 == rs2);
    assert(instruction.imm == imm);
    assert(instruction.uses_rs1 == uses_rs1);
    assert(instruction.uses_rs2 == uses_rs2);
    assert(instruction.writes_rd == writes_rd);
}

void check_invalid(uint32_t raw) {
    DecodedInstruction instruction = Decoder::decode(raw);

    assert(instruction.raw == raw);
    assert(!instruction.valid());
    assert(instruction.op == Operation::Invalid);
}

void test_u_type() {
    check_instruction(
        encode_u(0x12345U, 5U, 0x37U),
        Operation::LUI, InstructionFormat::U,
        5, 0, 0, 0x12345000,
        false, false, true
    );

    check_instruction(
        encode_u(0xFFFFFU, 6U, 0x17U),
        Operation::AUIPC, InstructionFormat::U,
        6, 0, 0, -4096,
        false, false, true
    );
}

void test_jump() {
    check_instruction(
        encode_j(16, 1U, 0x6FU),
        Operation::JAL, InstructionFormat::J,
        1, 0, 0, 16,
        false, false, true
    );

    check_instruction(
        encode_j(-16, 1U, 0x6FU),
        Operation::JAL, InstructionFormat::J,
        1, 0, 0, -16,
        false, false, true
    );

    check_instruction(
        encode_i(8, 5U, 0x0U, 1U, 0x67U),
        Operation::JALR, InstructionFormat::I,
        1, 5, 0, 8,
        true, false, true
    );
}

void test_branch() {
    check_instruction(
        encode_b(16, 6U, 5U, 0x0U, 0x63U),
        Operation::BEQ, InstructionFormat::B,
        0, 5, 6, 16,
        true, true, false
    );

    check_instruction(
        encode_b(-16, 6U, 5U, 0x1U, 0x63U),
        Operation::BNE, InstructionFormat::B,
        0, 5, 6, -16,
        true, true, false
    );

    check_instruction(
        encode_b(8, 6U, 5U, 0x4U, 0x63U),
        Operation::BLT, InstructionFormat::B,
        0, 5, 6, 8,
        true, true, false
    );

    check_instruction(
        encode_b(-8, 6U, 5U, 0x5U, 0x63U),
        Operation::BGE, InstructionFormat::B,
        0, 5, 6, -8,
        true, true, false
    );

    check_instruction(
        encode_b(12, 6U, 5U, 0x6U, 0x63U),
        Operation::BLTU, InstructionFormat::B,
        0, 5, 6, 12,
        true, true, false
    );

    check_instruction(
        encode_b(-12, 6U, 5U, 0x7U, 0x63U),
        Operation::BGEU, InstructionFormat::B,
        0, 5, 6, -12,
        true, true, false
    );
}

void test_load() {
    check_instruction(
        encode_i(-4, 5U, 0x0U, 6U, 0x03U),
        Operation::LB, InstructionFormat::I,
        6, 5, 0, -4,
        true, false, true
    );

    check_instruction(
        encode_i(2, 5U, 0x1U, 6U, 0x03U),
        Operation::LH, InstructionFormat::I,
        6, 5, 0, 2,
        true, false, true
    );

    check_instruction(
        encode_i(8, 5U, 0x2U, 6U, 0x03U),
        Operation::LW, InstructionFormat::I,
        6, 5, 0, 8,
        true, false, true
    );

    check_instruction(
        encode_i(-1, 5U, 0x4U, 6U, 0x03U),
        Operation::LBU, InstructionFormat::I,
        6, 5, 0, -1,
        true, false, true
    );

    check_instruction(
        encode_i(10, 5U, 0x5U, 6U, 0x03U),
        Operation::LHU, InstructionFormat::I,
        6, 5, 0, 10,
        true, false, true
    );
}

void test_store() {
    check_instruction(
        encode_s(-4, 6U, 5U, 0x0U, 0x23U),
        Operation::SB, InstructionFormat::S,
        0, 5, 6, -4,
        true, true, false
    );

    check_instruction(
        encode_s(2, 6U, 5U, 0x1U, 0x23U),
        Operation::SH, InstructionFormat::S,
        0, 5, 6, 2,
        true, true, false
    );

    check_instruction(
        encode_s(8, 6U, 5U, 0x2U, 0x23U),
        Operation::SW, InstructionFormat::S,
        0, 5, 6, 8,
        true, true, false
    );
}

void test_op_imm() {
    check_instruction(
        encode_i(-4, 6U, 0x0U, 5U, 0x13U),
        Operation::ADDI, InstructionFormat::I,
        5, 6, 0, -4,
        true, false, true
    );

    check_instruction(
        encode_i(7, 6U, 0x2U, 5U, 0x13U),
        Operation::SLTI, InstructionFormat::I,
        5, 6, 0, 7,
        true, false, true
    );

    check_instruction(
        encode_i(-1, 6U, 0x3U, 5U, 0x13U),
        Operation::SLTIU, InstructionFormat::I,
        5, 6, 0, -1,
        true, false, true
    );

    check_instruction(
        encode_i(0x55, 6U, 0x4U, 5U, 0x13U),
        Operation::XORI, InstructionFormat::I,
        5, 6, 0, 0x55,
        true, false, true
    );

    check_instruction(
        encode_i(0x123, 6U, 0x6U, 5U, 0x13U),
        Operation::ORI, InstructionFormat::I,
        5, 6, 0, 0x123,
        true, false, true
    );

    check_instruction(
        encode_i(-16, 6U, 0x7U, 5U, 0x13U),
        Operation::ANDI, InstructionFormat::I,
        5, 6, 0, -16,
        true, false, true
    );

    check_instruction(
        encode_i(3, 6U, 0x1U, 5U, 0x13U),
        Operation::SLLI, InstructionFormat::I,
        5, 6, 0, 3,
        true, false, true
    );

    check_instruction(
        encode_i(3, 6U, 0x5U, 5U, 0x13U),
        Operation::SRLI, InstructionFormat::I,
        5, 6, 0, 3,
        true, false, true
    );

    check_instruction(
        encode_i(0x403, 6U, 0x5U, 5U, 0x13U),
        Operation::SRAI, InstructionFormat::I,
        5, 6, 0, 3,
        true, false, true
    );
}

void test_op() {
    check_instruction(encode_r(0x00U, 7U, 6U, 0x0U, 5U, 0x33U), Operation::ADD, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x20U, 7U, 6U, 0x0U, 5U, 0x33U), Operation::SUB, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x1U, 5U, 0x33U), Operation::SLL, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x2U, 5U, 0x33U), Operation::SLT, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x3U, 5U, 0x33U), Operation::SLTU, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x4U, 5U, 0x33U), Operation::XOR, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x5U, 5U, 0x33U), Operation::SRL, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x20U, 7U, 6U, 0x5U, 5U, 0x33U), Operation::SRA, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x6U, 5U, 0x33U), Operation::OR, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
    check_instruction(encode_r(0x00U, 7U, 6U, 0x7U, 5U, 0x33U), Operation::AND, InstructionFormat::R, 5, 6, 7, 0, true, true, true);
}

void test_invalid() {
    check_invalid(0xFFFFFFFFU);
    check_invalid(0x00000073U);

    uint32_t invalid_jalr = encode_i(0, 1U, 0x1U, 1U, 0x67U);
    uint32_t invalid_load = encode_i(0, 1U, 0x3U, 1U, 0x03U);
    uint32_t invalid_slli = encode_i(0x23, 1U, 0x1U, 1U, 0x13U);
    uint32_t unsupported_mul = encode_r(0x01U, 3U, 2U, 0x0U, 1U, 0x33U);

    check_invalid(invalid_jalr);
    check_invalid(invalid_load);
    check_invalid(invalid_slli);
    check_invalid(unsupported_mul);
}

int main() {
    test_u_type();
    test_jump();
    test_branch();
    test_load();
    test_store();
    test_op_imm();
    test_op();
    test_invalid();

    std::cout << "All decoder tests passed.\n";
    return 0;
}