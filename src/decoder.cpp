#include "decoder.h"

#include <memory>

int32_t Decoder::sign_extend(uint32_t value, int bit_count) {
    const uint32_t sign_bit = 1U << (bit_count - 1); // 符号位
    const uint32_t mask = (1U << bit_count) - 1U;
    value &= mask;
    int32_t result = static_cast<int32_t>(value);
    if ((value & sign_bit) != 0U) {
        result -= static_cast<int32_t>(1U << bit_count);
    }
    return result;
}

/* i-type: raw[31:20] → imm[11:0]*/
int32_t Decoder::decode_i_immediate(uint32_t raw) {
    uint32_t immediate_bits = (raw >> 20U) & 0xFFFU;
    return sign_extend(immediate_bits, 12U);
}

/* s-type: raw[31:25] → imm[11:5]
           raw[11:7]  → imm[4:0] */
int32_t Decoder::decode_s_immediate(uint32_t raw) {
    const uint32_t imm_11_5 = (raw >> 25U) & 0x7FU;
    const uint32_t imm_4_0 = (raw >> 7U) & 0x1FU;
    const uint32_t imm_bits = (imm_11_5 << 5U) | imm_4_0;
    return sign_extend(imm_bits, 12U);
}

/* b-type: raw[31]    → imm[12]
           raw[30:25] → imm[10:5]
           raw[11:8]  → imm[4:1]
           raw[7]     → imm[11]
           imm[0]     → 0 */
int32_t Decoder::decode_b_immediate(uint32_t raw) {
    const uint32_t imm_12 = ((raw >> 31U) & 0x1U) << 12U;
    const uint32_t imm_10_5 = ((raw >> 25U) & 0x3FU) << 5U;
    const uint32_t imm_4_1 = ((raw >> 8U) & 0xFU) << 1U;
    const uint32_t imm_11 = ((raw >> 7U) & 0x1U) << 11U;
    const std::uint32_t imm_bits = imm_12 | imm_11 | imm_10_5 | imm_4_1;
    return sign_extend(imm_bits, 13U);
}

/* u-type: raw[31:12] → imm[31:12]
           imm[11:0]  → 全部为 0 */
int32_t Decoder::decode_u_immediate(uint32_t raw) {
    const uint32_t upper_20_bits = (raw >> 12U) & 0xFFFFFU;
    const int32_t signed_upper = sign_extend(upper_20_bits, 20U);
    const int64_t full_value = static_cast<int64_t>(signed_upper) * 4096;
    return static_cast<int32_t>(full_value);
}

/* j-type: raw[31]    → imm[20]
           raw[30:21] → imm[10:1]
           raw[20]    → imm[11]
           raw[19:12] → imm[19:12]
           imm[0]     → 0 */
std::int32_t Decoder::decode_j_immediate(std::uint32_t raw) {
    const uint32_t imm_20 = (raw >> 31U & 0x1U) << 20U;
    const uint32_t imm_10_1 = (raw >> 21U & 0x3FFU) << 1U;
    const uint32_t imm_11 = (raw >> 20U & 0x1U) << 11U;
    const uint32_t imm_19_12 = (raw >> 12U & 0xFFU) << 12U;
    const uint32_t imm_bits = imm_10_1 | imm_11 | imm_19_12 | imm_20;
    return sign_extend(imm_bits, 21U);
}

DecodedInstruction Decoder::decode_op_imm(uint32_t raw) {
    DecodedInstruction instruction{};
    instruction.raw = raw;
    instruction.format = InstructionFormat::I;
    instruction.rd = static_cast<uint8_t>(raw >> 7U & 0x1FU);
    instruction.rs1 = static_cast<uint8_t>(raw >> 15U & 0x1FU);
    instruction.uses_rs1 = true;
    instruction.uses_rs2 = false;
    instruction.writes_rd = true;
    const uint32_t funct3 = (raw >> 12U) & 0x7U;
    const uint32_t funct7 = (raw >> 25U) & 0x7FU;

    switch (funct3) {
    case 0x0U:
        instruction.op = Operation::ADDI;
        instruction.imm = decode_i_immediate(raw);
        break;
    case 0x2U:
        instruction.op = Operation::SLTI;
        instruction.imm = decode_i_immediate(raw);
        break;
    case 0x3U:
        instruction.op = Operation::SLTIU;
        instruction.imm = decode_i_immediate(raw);
        break;
    case 0x4U:
        instruction.op = Operation::XORI;
        instruction.imm = decode_i_immediate(raw);
        break;
    case 0x6U:
        instruction.op = Operation::ORI;
        instruction.imm = decode_i_immediate(raw);
        break;
    case 0x7U:
        instruction.op = Operation::ANDI;
        instruction.imm = decode_i_immediate(raw);
        break;
    case 0x1U:
        if (funct7 == 0x0U) {
            instruction.op = Operation::SLLI;
            instruction.imm = static_cast<int32_t>(raw >> 20U & 0x1FU); // 保存shamt
        }
        break;
    case 0x5U:
        if (funct7 == 0x00U) {
            instruction.op = Operation::SRLI;
            instruction.imm = static_cast<int32_t>(raw >> 20U & 0x1FU); // 保存shamt
        }
        else if (funct7 == 0x20U) {
            instruction.op = Operation::SRAI;
            instruction.imm = static_cast<int32_t>(raw >> 20U & 0x1FU); // 保存shamt
        }
        break;

    default:
        break;
    }
    return instruction;
}

DecodedInstruction Decoder::decode(uint32_t raw) {
    const uint32_t opcode = raw & 0x7FU;
    switch (opcode) {
    case 0x37U:
        return decode_lui(raw);
    case 0x17U:
        return decode_auipc(raw);
    case 0x6FU:
        return decode_jal(raw);
    case 0x67U:
        return decode_jalr(raw);
    case 0x63U:
        return decode_branch(raw);
    case 0x03U:
        return decode_load(raw);
    case 0x23U:
        return decode_store(raw);
    case 0x13U:
        return decode_op_imm(raw);
    case 0x33U:
        return decode_op(raw);
    default: {
        DecodedInstruction instruction{};
        instruction.raw = raw;
        return instruction;
    }
    }
}

DecodedInstruction Decoder::decode_lui(uint32_t raw) {
    DecodedInstruction instruction{};

    instruction.raw = raw;
    instruction.op = Operation::LUI;
    instruction.format = InstructionFormat::U;
    instruction.rd = static_cast<uint8_t>((raw >> 7U) & 0x1FU);
    instruction.imm = decode_u_immediate(raw);
    instruction.uses_rs1 = false;
    instruction.uses_rs2 = false;
    instruction.writes_rd = true;

    return instruction;
}

DecodedInstruction Decoder::decode_auipc(uint32_t raw) {
    DecodedInstruction instruction{};

    instruction.raw = raw;
    instruction.op = Operation::AUIPC;
    instruction.format = InstructionFormat::U;
    instruction.rd = static_cast<uint8_t>((raw >> 7U) & 0x1FU);
    instruction.imm = decode_u_immediate(raw);
    instruction.uses_rs1 = false;
    instruction.uses_rs2 = false;
    instruction.writes_rd = true;

    return instruction;
}

DecodedInstruction Decoder::decode_jal(uint32_t raw) {
    DecodedInstruction instruction{};

    instruction.raw = raw;
    instruction.op = Operation::JAL;
    instruction.format = InstructionFormat::J;
    instruction.rd = static_cast<uint8_t>((raw >> 7U) & 0x1FU);
    instruction.imm = decode_j_immediate(raw);
    instruction.uses_rs1 = false;
    instruction.uses_rs2 = false;
    instruction.writes_rd = true;

    return instruction;
}

DecodedInstruction Decoder::decode_jalr(uint32_t raw) {
    DecodedInstruction instruction{};
    instruction.raw = raw;

    const uint32_t funct3 = (raw >> 12U) & 0x7U;
    if (funct3 != 0x0U) {
        return instruction;
    }

    instruction.op = Operation::JALR;
    instruction.format = InstructionFormat::I;
    instruction.rd = static_cast<uint8_t>((raw >> 7U) & 0x1FU);
    instruction.rs1 = static_cast<uint8_t>((raw >> 15U) & 0x1FU);
    instruction.imm = decode_i_immediate(raw);
    instruction.uses_rs1 = true;
    instruction.uses_rs2 = false;
    instruction.writes_rd = true;

    return instruction;
}

DecodedInstruction Decoder::decode_branch(uint32_t raw) {
    DecodedInstruction instruction{};
    instruction.raw = raw;

    const uint32_t funct3 = (raw >> 12U) & 0x7U;

    switch (funct3) {
    case 0x0U:
        instruction.op = Operation::BEQ;
        break;

    case 0x1U:
        instruction.op = Operation::BNE;
        break;

    case 0x4U:
        instruction.op = Operation::BLT;
        break;

    case 0x5U:
        instruction.op = Operation::BGE;
        break;

    case 0x6U:
        instruction.op = Operation::BLTU;
        break;

    case 0x7U:
        instruction.op = Operation::BGEU;
        break;

    default:
        return instruction;
    }

    instruction.format = InstructionFormat::B;
    instruction.rs1 = static_cast<uint8_t>((raw >> 15U) & 0x1FU);
    instruction.rs2 = static_cast<uint8_t>((raw >> 20U) & 0x1FU);
    instruction.imm = decode_b_immediate(raw);
    instruction.uses_rs1 = true;
    instruction.uses_rs2 = true;
    instruction.writes_rd = false;

    return instruction;
}

DecodedInstruction Decoder::decode_load(uint32_t raw) {
    DecodedInstruction instruction{};
    instruction.raw = raw;

    const uint32_t funct3 = (raw >> 12U) & 0x7U;

    switch (funct3) {
    case 0x0U:
        instruction.op = Operation::LB;
        break;

    case 0x1U:
        instruction.op = Operation::LH;
        break;

    case 0x2U:
        instruction.op = Operation::LW;
        break;

    case 0x4U:
        instruction.op = Operation::LBU;
        break;

    case 0x5U:
        instruction.op = Operation::LHU;
        break;

    default:
        return instruction;
    }

    instruction.format = InstructionFormat::I;
    instruction.rd = static_cast<uint8_t>((raw >> 7U) & 0x1FU);
    instruction.rs1 = static_cast<uint8_t>((raw >> 15U) & 0x1FU);
    instruction.imm = decode_i_immediate(raw);
    instruction.uses_rs1 = true;
    instruction.uses_rs2 = false;
    instruction.writes_rd = true;

    return instruction;
}

DecodedInstruction Decoder::decode_store(uint32_t raw) {
    DecodedInstruction instruction{};
    instruction.raw = raw;

    const uint32_t funct3 = (raw >> 12U) & 0x7U;

    switch (funct3) {
    case 0x0U:
        instruction.op = Operation::SB;
        break;

    case 0x1U:
        instruction.op = Operation::SH;
        break;

    case 0x2U:
        instruction.op = Operation::SW;
        break;

    default:
        return instruction;
    }

    instruction.format = InstructionFormat::S;
    instruction.rs1 = static_cast<uint8_t>((raw >> 15U) & 0x1FU);
    instruction.rs2 = static_cast<uint8_t>((raw >> 20U) & 0x1FU);
    instruction.imm = decode_s_immediate(raw);
    instruction.uses_rs1 = true;
    instruction.uses_rs2 = true;
    instruction.writes_rd = false;

    return instruction;
}

DecodedInstruction Decoder::decode_op(uint32_t raw) {
    DecodedInstruction instruction{};
    instruction.raw = raw;

    const uint32_t funct3 = (raw >> 12U) & 0x7U;
    const uint32_t funct7 = (raw >> 25U) & 0x7FU;

    switch (funct3) {
    case 0x0U:
        if (funct7 == 0x00U) {
            instruction.op = Operation::ADD;
        }
        else if (funct7 == 0x20U) {
            instruction.op = Operation::SUB;
        }
        else {
            return instruction;
        }
        break;

    case 0x1U:
        if (funct7 != 0x00U) {
            return instruction;
        }
        instruction.op = Operation::SLL;
        break;

    case 0x2U:
        if (funct7 != 0x00U) {
            return instruction;
        }
        instruction.op = Operation::SLT;
        break;

    case 0x3U:
        if (funct7 != 0x00U) {
            return instruction;
        }
        instruction.op = Operation::SLTU;
        break;

    case 0x4U:
        if (funct7 != 0x00U) {
            return instruction;
        }
        instruction.op = Operation::XOR;
        break;

    case 0x5U:
        if (funct7 == 0x00U) {
            instruction.op = Operation::SRL;
        }
        else if (funct7 == 0x20U) {
            instruction.op = Operation::SRA;
        }
        else {
            return instruction;
        }
        break;

    case 0x6U:
        if (funct7 != 0x00U) {
            return instruction;
        }
        instruction.op = Operation::OR;
        break;

    case 0x7U:
        if (funct7 != 0x00U) {
            return instruction;
        }
        instruction.op = Operation::AND;
        break;

    default:
        return instruction;
    }

    instruction.format = InstructionFormat::R;
    instruction.rd = static_cast<uint8_t>((raw >> 7U) & 0x1FU);
    instruction.rs1 = static_cast<uint8_t>((raw >> 15U) & 0x1FU);
    instruction.rs2 = static_cast<uint8_t>((raw >> 20U) & 0x1FU);
    instruction.uses_rs1 = true;
    instruction.uses_rs2 = true;
    instruction.writes_rd = true;

    return instruction;
}
