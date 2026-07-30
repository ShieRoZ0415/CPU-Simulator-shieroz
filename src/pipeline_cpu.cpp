#include "pipeline_cpu.h"

PipelineCpu::PipelineCpu(Memory& memory) noexcept : memory_(memory) {}

void PipelineCpu::reset() noexcept {
    registers_.reset();
    pc_ = 0U;
    cycle_count_ = 0U;
    fetch_stopped_ = false;
    if_id_ = {};
    id_ex_ = {};
    ex_mem_ = {};
    mem_wb_ = {};
}

uint32_t PipelineCpu::pc() const noexcept {
    return pc_;
}

uint32_t PipelineCpu::register_value(uint8_t index) const noexcept {
    return registers_.read(index);
}

int64_t PipelineCpu::signed_value(uint32_t value) noexcept {
    if ((value & 0x80000000) != 0U) return static_cast<int64_t>(value) - 0x100000000LL;
    return static_cast<int64_t>(value);
}

uint32_t PipelineCpu::sign_extend_to_u32(uint32_t value, unsigned int bit_count) noexcept {
    const uint32_t sign_bit = 1U << (bit_count - 1U);
    const uint32_t mask = (1U << bit_count) - 1U;
    value &= mask;
    if ((value & sign_bit) != 0U) {
        return value | ~mask;
    }
    return value;
}

uint32_t PipelineCpu::arithmetic_shift_right(uint32_t value, uint32_t shift_amount) noexcept {
    shift_amount &= 0x1FU;
    if (shift_amount == 0U) return value;
    uint32_t result = value >> shift_amount;
    if ((value & 0x80000000U) != 0U) {
        const uint32_t mask = 0xFFFFFFFFU << (32U - shift_amount);
        result |= mask;
    }
    return result;
}

bool PipelineCpu::is_load(Operation op) noexcept {
    return op == Operation::LB || op == Operation::LBU || op == Operation::LH
        || op == Operation::LHU || op == Operation::LW;
}

bool PipelineCpu::is_memory(Operation op) noexcept {
    return op == Operation::LB || op == Operation::LH || op == Operation::LW
        || op == Operation::LBU || op == Operation::LHU || op == Operation::SB
        || op == Operation::SH || op == Operation::SW;
}

bool PipelineCpu::write_register(const DecodedInstruction& instruction) noexcept {
    return instruction.writes_rd && instruction.rd != 0U;
}

uint64_t PipelineCpu::cycle_count() const noexcept {
    return cycle_count_;
}

uint32_t PipelineCpu::forwarded_rs1(const IdExRegister& current) const noexcept {
    if (!current.instruction.uses_rs1) return 0U;
    uint32_t value = current.rs1_value;
    if (mem_wb_.valid && write_register(mem_wb_.instruction) && mem_wb_.instruction.rd == current.instruction.rs1) {
        value = mem_wb_.write_value;
    }
    if (ex_mem_.valid && write_register(ex_mem_.instruction) && !is_load(ex_mem_.instruction.op) && ex_mem_.instruction.
        rd == current.instruction.rs1) {
        value = ex_mem_.result;
    }
    return value;
}

uint32_t PipelineCpu::forwarded_rs2(const IdExRegister& current) const noexcept {
    if (!current.instruction.uses_rs2) return 0U;
    uint32_t value = current.rs2_value;
    if (mem_wb_.valid && write_register(mem_wb_.instruction) && mem_wb_.instruction.rd == current.instruction.rs2) {
        value = mem_wb_.write_value;
    }
    if (ex_mem_.valid && write_register(ex_mem_.instruction) && !is_load(ex_mem_.instruction.op) && ex_mem_.instruction.
        rd == current.instruction.rs2) {
        value = ex_mem_.result;
    }
    return value;
}

bool PipelineCpu::needs_stall(const DecodedInstruction& instruction) const noexcept {
    if (!id_ex_.valid) return false;
    if (!is_load(id_ex_.instruction.op)) return false;
    if (id_ex_.instruction.rd == 0U) return false;
    if (instruction.uses_rs1 && instruction.rs1 == id_ex_.instruction.rd) return true;
    if (instruction.uses_rs2 && instruction.rs2 == id_ex_.instruction.rd) return true;
    return false;
}


PipelineStepResult PipelineCpu::step_cycle() {
    ++cycle_count_;
    IfIdRegister next_if_id{};
    IdExRegister next_id_ex{};
    ExMemRegister next_ex_mem{};
    MemWbRegister next_mem_wb{};
    bool redirect_pc = false;
    uint32_t redirect_target_pc = 0U;

    // WB阶段
    if (mem_wb_.valid && write_register(mem_wb_.instruction)) {
        registers_.write(mem_wb_.instruction.rd, mem_wb_.write_value);
    }
    // 三周期访存延迟
    const bool memory_waiting = ex_mem_.valid && is_memory(ex_mem_.instruction.op) && ex_mem_.mem_cycles > 1U;
    if (memory_waiting) {
        next_ex_mem = ex_mem_;
        --next_ex_mem.mem_cycles;
        next_id_ex = id_ex_;
        next_if_id = if_id_;
        if (next_id_ex.valid) {
            next_id_ex.rs1_value = forwarded_rs1(id_ex_);
            next_id_ex.rs2_value = forwarded_rs2(id_ex_);
        }
        if_id_ = next_if_id;
        id_ex_ = next_id_ex;
        ex_mem_ = next_ex_mem;
        mem_wb_ = next_mem_wb; // bubble

        return PipelineStepResult::Continue;
    }

    // MEM 阶段
    if (ex_mem_.valid) {
        next_mem_wb.valid = true;
        next_mem_wb.instruction = ex_mem_.instruction;
        next_mem_wb.write_value = ex_mem_.result;
        switch (ex_mem_.instruction.op) {
        case Operation::LB:
            next_mem_wb.write_value = sign_extend_to_u32(memory_.read_byte(ex_mem_.result), 8U);
            break;
        case Operation::LH:
            next_mem_wb.write_value = sign_extend_to_u32(memory_.read_half(ex_mem_.result), 16U);
            break;
        case Operation::LW:
            next_mem_wb.write_value = memory_.read_word(ex_mem_.result);
            break;
        case Operation::LHU:
            next_mem_wb.write_value = memory_.read_half(ex_mem_.result);
            break;
        case Operation::LBU:
            next_mem_wb.write_value = memory_.read_byte(ex_mem_.result);
            break;
        case Operation::SB:
            memory_.write_byte(ex_mem_.result, static_cast<uint8_t>(ex_mem_.store_data & 0xFFU));
            break;
        case Operation::SH:
            memory_.write_half(ex_mem_.result, static_cast<uint16_t>(ex_mem_.store_data & 0xFFFFU));
            break;
        case Operation::SW:
            memory_.write_word(ex_mem_.result, ex_mem_.store_data);
            break;
        default:
            break;
        }
    }

    // EX 阶段
    if (id_ex_.valid) {
        next_ex_mem.valid = true;
        next_ex_mem.instruction = id_ex_.instruction;
        if (is_memory(id_ex_.instruction.op)) {
            next_ex_mem.mem_cycles = MemoryCycles;
        }
        const uint32_t rs1_value = forwarded_rs1(id_ex_);
        const uint32_t rs2_value = forwarded_rs2(id_ex_);
        const auto immediate = static_cast<uint32_t>(id_ex_.instruction.imm);
        next_ex_mem.store_data = rs2_value;

        switch (id_ex_.instruction.op) {
        case Operation::LUI:
            next_ex_mem.result = immediate;
            break;
        case Operation::AUIPC:
            next_ex_mem.result = id_ex_.pc + immediate;
            break;
        case Operation::JAL:
            next_ex_mem.result = id_ex_.pc + 4U;
            redirect_pc = true;
            redirect_target_pc = id_ex_.pc + immediate;
            break;
        case Operation::JALR:
            next_ex_mem.result = id_ex_.pc + 4U;
            redirect_pc = true;
            redirect_target_pc = (rs1_value + immediate) & ~1U;
            break;
        case Operation::BEQ:
            if (rs1_value == rs2_value) {
                redirect_pc = true;
                redirect_target_pc = id_ex_.pc + immediate;
            }
            break;
        case Operation::BNE:
            if (rs1_value != rs2_value) {
                redirect_pc = true;
                redirect_target_pc = id_ex_.pc + immediate;
            }
            break;
        case Operation::BLT:
            if (signed_value(rs1_value) < signed_value(rs2_value)) {
                redirect_pc = true;
                redirect_target_pc = id_ex_.pc + immediate;
            }
            break;
        case Operation::BGE:
            if (signed_value(rs1_value) >= signed_value(rs2_value)) {
                redirect_pc = true;
                redirect_target_pc = id_ex_.pc + immediate;
            }
            break;
        case Operation::BLTU:
            if (rs1_value < rs2_value) {
                redirect_pc = true;
                redirect_target_pc = id_ex_.pc + immediate;
            }
            break;
        case Operation::BGEU:
            if (rs1_value >= rs2_value) {
                redirect_pc = true;
                redirect_target_pc = id_ex_.pc + immediate;
            }
            break;
        case Operation::LB:
        case Operation::LH:
        case Operation::LW:
        case Operation::LBU:
        case Operation::LHU:
        case Operation::SB:
        case Operation::SH:
        case Operation::SW:
            next_ex_mem.result = rs1_value + immediate;
            break;
        case Operation::ADDI:
            next_ex_mem.result = rs1_value + immediate;
            break;
        case Operation::SLTI:
            next_ex_mem.result = signed_value(rs1_value) < static_cast<int64_t>(id_ex_.instruction.imm) ? 1U : 0U;
            break;
        case Operation::SLTIU:
            next_ex_mem.result = rs1_value < immediate ? 1U : 0U;
            break;
        case Operation::XORI:
            next_ex_mem.result = rs1_value ^ immediate;
            break;
        case Operation::ORI:
            next_ex_mem.result = rs1_value | immediate;
            break;
        case Operation::ANDI:
            next_ex_mem.result = rs1_value & immediate;
            break;
        case Operation::SLLI:
            next_ex_mem.result = rs1_value << (immediate & 0x1FU);
            break;
        case Operation::SRLI:
            next_ex_mem.result = rs1_value >> (immediate & 0x1FU);
            break;
        case Operation::SRAI:
            next_ex_mem.result = arithmetic_shift_right(rs1_value, immediate);
            break;
        case Operation::ADD:
            next_ex_mem.result = rs1_value + rs2_value;
            break;
        case Operation::SUB:
            next_ex_mem.result = rs1_value - rs2_value;
            break;
        case Operation::SLL:
            next_ex_mem.result = rs1_value << (rs2_value & 0x1FU);
            break;
        case Operation::SLT:
            next_ex_mem.result = signed_value(rs1_value) < signed_value(rs2_value) ? 1U : 0U;
            break;
        case Operation::SLTU:
            next_ex_mem.result = rs1_value < rs2_value ? 1U : 0U;
            break;
        case Operation::XOR:
            next_ex_mem.result = rs1_value ^ rs2_value;
            break;
        case Operation::SRL:
            next_ex_mem.result = rs1_value >> (rs2_value & 0x1FU);
            break;
        case Operation::SRA:
            next_ex_mem.result = arithmetic_shift_right(rs1_value, rs2_value);
            break;
        case Operation::OR:
            next_ex_mem.result = rs1_value | rs2_value;
            break;
        case Operation::AND:
            next_ex_mem.result = rs1_value & rs2_value;
            break;
        case Operation::Invalid:
        default:
            return PipelineStepResult::InvalidInstruction;
        }
    }

    // 控制冒险
    if (redirect_pc) {
        pc_ = redirect_target_pc;
        fetch_stopped_ = false;
    }
    else {
        bool stall = false;
        // ID 阶段
        if (if_id_.valid) {
            const DecodedInstruction decoded = Decoder::decode(if_id_.raw);
            if (!decoded.valid()) {
                return PipelineStepResult::InvalidInstruction;
            }
            stall = needs_stall(decoded);
            if (!stall) {
                next_id_ex.valid = true;
                next_id_ex.pc = if_id_.pc;
                next_id_ex.instruction = decoded;
                if (decoded.uses_rs1) next_id_ex.rs1_value = registers_.read(decoded.rs1);
                if (decoded.uses_rs2) next_id_ex.rs2_value = registers_.read(decoded.rs2);
            }
        }

        // IF 阶段
        if (stall) {
            next_if_id = if_id_;
        }
        else if (!fetch_stopped_) {
            const uint32_t raw = memory_.read_word(pc_);
            if (raw ==HaltInstruction) fetch_stopped_ = true;
            else {
                next_if_id.valid = true;
                next_if_id.pc = pc_;
                next_if_id.raw = raw;
                pc_ += 4U;
            }
        }
    }

    if_id_ = next_if_id;
    id_ex_ = next_id_ex;
    ex_mem_ = next_ex_mem;
    mem_wb_ = next_mem_wb;

    if (fetch_stopped_ && !if_id_.valid && !id_ex_.valid && !ex_mem_.valid && !mem_wb_.valid) return PipelineStepResult::Halted;

    return PipelineStepResult::Continue;
}
