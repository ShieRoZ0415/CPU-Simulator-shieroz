#include "cpu.h"

Cpu::Cpu(Memory& memory) noexcept : memory_(memory){}

void Cpu::reset() noexcept {
    registers_.reset();
    pc_ = 0U;
}

uint32_t Cpu::pc() const noexcept {
    return pc_;
}

uint32_t Cpu::register_value(uint8_t index) const noexcept {
    return registers_.read(index);
}

int64_t Cpu::signed_value(uint32_t value) noexcept {
    if ((value & 0x80000000) != 0U) return static_cast<int64_t>(value) - 0x100000000LL;
    return static_cast<int64_t>(value);
}

uint32_t Cpu::sign_extend_to_u32(uint32_t value, unsigned int bit_count) noexcept {
    const uint32_t sign_bit = 1U << (bit_count - 1U);
    const uint32_t mask = (1U << bit_count) - 1U;
    value &= mask;
    if ((value & sign_bit) != 0U) {
        return value | ~mask;
    }
    return value;
}

uint32_t Cpu::arithmetic_shift_right(uint32_t value, uint32_t shift_amount) noexcept {
    shift_amount &= 0x1FU;
    if (shift_amount == 0U) return value;
    uint32_t result = value >> shift_amount;
    if ((value & 0x80000000U) != 0U) {
        uint32_t fill = 0xFFFFFFFFU;
        result |= fill;
    }
    return result;
}

bool Cpu::is_memory(Operation op) noexcept {
    return op == Operation::LB ||
           op == Operation::LH ||
           op == Operation::LW ||
           op == Operation::LBU ||
           op == Operation::LHU ||
           op == Operation::SB ||
           op == Operation::SH ||
           op == Operation::SW;
}

uint64_t Cpu::cycle_count() const noexcept {
    return cycle_count_;
}

StepResult Cpu::step() {
    const uint32_t raw = memory_.read_word(pc_);

    if (raw == HaltInstruction) return StepResult::Halted;

    const DecodedInstruction instruction = Decoder::decode(raw);
    if (!instruction.valid()) return StepResult::InvalidInstruction;
    if (is_memory(instruction.op)) {
        cycle_count_ += 3U;
    } else {
        cycle_count_ += 1U;
    }
    const uint32_t rs1_value = instruction.uses_rs1 ? registers_.read(instruction.rs1) : 0U;
    const uint32_t rs2_value = instruction.uses_rs2 ? registers_.read(instruction.rs2) : 0U;
    const auto immediate = static_cast<uint32_t>(instruction.imm);
    uint32_t pc_next = pc_ + 4U;

    switch (instruction.op) {
    case Operation::LUI:
        registers_.write(instruction.rd, immediate);
        break;
    case Operation::AUIPC:
        registers_.write(instruction.rd, pc_ + immediate);
        break;
    case Operation::JAL:
        registers_.write(instruction.rd, pc_ + 4U);
        pc_next = pc_ + immediate;
        break;
    case Operation::JALR:
        registers_.write(instruction.rd, pc_ + 4U);
        pc_next = pc_ + (rs1_value + immediate) & ~1U;
        break;
    case Operation::BEQ:
        if (rs1_value == rs2_value) pc_next = pc_ + immediate;
        break;
    case Operation::BNE:
        if (rs1_value != rs2_value) pc_next = pc_ + immediate;
        break;
    case Operation::BLT:
        if (signed_value(rs1_value) < signed_value(rs2_value)) pc_next = pc_ + immediate;
        break;
    case Operation::BGE:
        if (signed_value(rs1_value) >= signed_value(rs2_value)) pc_next = pc_ + immediate;
        break;
    case Operation::BLTU:
        if (rs1_value < rs2_value) pc_next = pc_ + immediate;
        break;
    case Operation::BGEU:
        if (rs1_value >= rs2_value) pc_next = pc_ + immediate;
        break;
    case Operation::LB: {
        const uint32_t address = rs1_value + immediate;
        const uint32_t value = memory_.read_byte(address);
        registers_.write(instruction.rd, sign_extend_to_u32(value, 8U));
        break;
    }
    case Operation::LH: {
        const uint32_t address = rs1_value + immediate;
        const uint32_t value = memory_.read_half(address);
        registers_.write(instruction.rd, sign_extend_to_u32(value, 16U));
        break;
    }
    case Operation::LW: {
        const uint32_t address = rs1_value + immediate;
        registers_.write(instruction.rd, memory_.read_word(address));
        break;
    }
    case Operation::LBU: {
        const uint32_t address = rs1_value + immediate;
        registers_.write(instruction.rd, memory_.read_byte(address));
        break;
    }
    case Operation::LHU: {
        const uint32_t address = rs1_value + immediate;
        registers_.write(instruction.rd, memory_.read_half(address));
        break;
    }
    case Operation::SB: {
        const uint32_t address = rs1_value + immediate;
        memory_.write_byte(address, static_cast<uint8_t>(rs2_value & 0xFFU));
        break;
    }
    case Operation::SH: {
        const uint32_t address = rs1_value + immediate;
        memory_.write_half(address, static_cast<uint16_t>(rs2_value & 0xFFFFU));
        break;
    }
    case Operation::SW: {
        const uint32_t address = rs1_value + immediate;
        memory_.write_word(address, rs2_value);
        break;
    }
    case Operation::ADD:
        registers_.write(instruction.rd, rs1_value + rs2_value);
        break;
    case Operation::SUB:
        registers_.write(instruction.rd, rs1_value - rs2_value);
        break;
    case Operation::SLL: {
        const uint32_t shift_amount = rs2_value & 0x1FU;
        registers_.write(instruction.rd, rs1_value << shift_amount);
        break;
    }
    case Operation::SRL: {
        const uint32_t shift_amount = rs2_value & 0x1FU;
        registers_.write(instruction.rd, rs1_value >> shift_amount);
        break;
    }
    case Operation::SLT:
        registers_.write(instruction.rd, signed_value(rs1_value) < signed_value(rs2_value) ? 1U : 0U);
        break;
    case Operation::SLTU:
        registers_.write(instruction.rd, rs1_value < rs2_value ? 1U : 0U);
        break;
    case Operation::SRA: {
        const uint32_t shift_amount = rs2_value & 0x1FU;
        registers_.write(instruction.rd, arithmetic_shift_right(rs1_value, shift_amount));
        break;
    }
    case Operation::AND:
        registers_.write(instruction.rd, rs1_value & rs2_value);
        break;
    case Operation::OR:
        registers_.write(instruction.rd, rs1_value | rs2_value);
        break;
    case Operation::XOR:
        registers_.write(instruction.rd, rs1_value ^ immediate);
        break;
    case Operation::ADDI:
        registers_.write(instruction.rd, rs1_value + immediate);
        break;
    case Operation::SLTI:
        registers_.write(instruction.rd, signed_value(rs1_value) < static_cast<int64_t>(instruction.imm) ? 1U : 0U);
        break;
    case Operation::SLTIU:
        registers_.write(instruction.rd, rs1_value < immediate ? 1U : 0U);
        break;
    case Operation::XORI:
        registers_.write(instruction.rd, rs1_value ^ immediate);
        break;
    case Operation::ORI:
        registers_.write(instruction.rd, rs1_value | immediate);
        break;
    case Operation::ANDI:
        registers_.write(instruction.rd, rs1_value & immediate);
        break;
    case Operation::SLLI: {
        const uint32_t shift_amount = immediate & 0x1FU;
        registers_.write(instruction.rd, rs1_value << shift_amount);
        break;
    }
    case Operation::SRLI: {
        const uint32_t shift_amount = immediate & 0x1FU;
        registers_.write(instruction.rd, rs1_value >> shift_amount);
        break;
    }
    case Operation::SRAI: {
        const uint32_t shift_amount = immediate & 0x1FU;
        registers_.write(instruction.rd, arithmetic_shift_right(rs1_value, shift_amount));
        break;
    }
    case Operation::Invalid:
    default:return StepResult::InvalidInstruction;
    }
    pc_ = pc_next;
    return StepResult::Continue;
}



