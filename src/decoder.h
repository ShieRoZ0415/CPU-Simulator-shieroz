#pragma once

#include <cstdint>

#include "instruction.h"

/* 解码规则: [31:20](普通op-imm):12位立即数
 *         [31:20](移位op-imm):[31:25]:funct7
 *                            [24:20]:5位移位量
 * [19:15]:rs1
 * [14:12]:funct3
 * [11:7] :rd
 * [6:0]  :opcode */

class Decoder {
public:
    static DecodedInstruction decode(uint32_t raw);

private:
    static int32_t sign_extend(uint32_t value, int bit_count);
    // 立即数重建
    static std::int32_t decode_i_immediate(std::uint32_t raw);
    static std::int32_t decode_s_immediate(std::uint32_t raw);
    static std::int32_t decode_b_immediate(std::uint32_t raw);
    static std::int32_t decode_u_immediate(std::uint32_t raw);
    static std::int32_t decode_j_immediate(std::uint32_t raw);

    // opcode 指令组
    static DecodedInstruction decode_lui(std::uint32_t raw);
    static DecodedInstruction decode_auipc(std::uint32_t raw);
    static DecodedInstruction decode_jal(std::uint32_t raw);
    static DecodedInstruction decode_jalr(std::uint32_t raw);
    static DecodedInstruction decode_branch(std::uint32_t raw);
    static DecodedInstruction decode_load(std::uint32_t raw);
    static DecodedInstruction decode_store(std::uint32_t raw);
    static DecodedInstruction decode_op_imm(std::uint32_t raw);
    static DecodedInstruction decode_op(std::uint32_t raw);
};