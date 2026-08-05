#include "tomasulo_cpu.h"

TomasuloCpu::TomasuloCpu(Memory& memory) noexcept : memory_(memory) {
    reset();
}

void TomasuloCpu::reset() noexcept {
    state_ = State{};
    state_.registers.reset();
    for (uint8_t i = 0U; i < RegisterCount; ++i) state_.rat_tag[i] = NoTag;
    for (uint16_t i = 0U; i < PredictorSize; ++i) state_.predictor[i] = 1U;
}

uint32_t TomasuloCpu::pc() const noexcept {
    return state_.pc;
}

uint32_t TomasuloCpu::register_value(uint8_t index) const noexcept {
    return state_.registers.read(index);
}

uint64_t TomasuloCpu::cycle_count() const noexcept {
    return state_.cycle_count;
}

uint64_t TomasuloCpu::branch_predictions() const noexcept {
    return state_.prediction_count;
}

uint64_t TomasuloCpu::correct_branch_predictions() const noexcept {
    return state_.correct_prediction_count;
}

double TomasuloCpu::branch_accuracy() const noexcept {
    if (state_.prediction_count == 0U) return 0.0;
    return static_cast<double>(state_.correct_prediction_count) / static_cast<double>(state_.prediction_count);
}

bool TomasuloCpu::writes_register(const DecodedInstruction& instruction) noexcept {
    return instruction.writes_rd && instruction.rd != 0U;
}

bool TomasuloCpu::is_load(Operation op) noexcept {
    return op == Operation::LB || op == Operation::LH || op == Operation::LW || op == Operation::LBU || op ==
        Operation::LHU;
}

bool TomasuloCpu::is_store(Operation op) noexcept {
    return op == Operation::SB || op == Operation::SH || op == Operation::SW;
}

bool TomasuloCpu::is_memory(Operation op) noexcept {
    return is_load(op) || is_store(op);
}

bool TomasuloCpu::is_branch(Operation op) noexcept {
    return op == Operation::BEQ || op == Operation::BNE || op == Operation::BLT ||
        op == Operation::BGE || op == Operation::BLTU || op == Operation::BGEU;
}

bool TomasuloCpu::is_control(Operation op) noexcept {
    return is_branch(op) || op == Operation::JAL || op == Operation::JALR;
}

int64_t TomasuloCpu::signed_value(uint32_t value) noexcept {
    if ((value & 0x80000000U) != 0U) return value - 0x100000000LL;
    return value;
}

uint32_t TomasuloCpu::sign_extend(uint32_t value, uint32_t bits) noexcept {
    if (bits >= 32U) return value;
    const uint32_t sign = 1U << (bits - 1U);
    const uint32_t mask = (1U << bits) - 1U;
    value &= mask;
    if ((value & sign) != 0U) value |= ~mask;
    return value;
}

uint32_t TomasuloCpu::arithmetic_shift_right(uint32_t value, uint32_t amount) noexcept {
    amount &= 0x1FU;
    if (amount == 0U) return value;
    uint32_t result = value >> amount;
    if ((value & 0x80000000U) != 0U) result |= 0xFFFFFFFFU << (32U - amount);
    return result;
}

uint32_t TomasuloCpu::execute_value(const RsEntry& entry) noexcept {
    const uint32_t lhs = entry.lhs.value;
    const uint32_t rhs = entry.rhs.value;
    const int32_t imm_s = entry.instruction.imm;
    const uint32_t imm_u = static_cast<uint32_t>(imm_s);
    switch (entry.instruction.op) {
    case Operation::LUI: return imm_u;
    case Operation::AUIPC: return entry.pc + imm_u;
    case Operation::JAL:
    case Operation::JALR: return entry.pc + 4U;
    case Operation::BEQ:
    case Operation::BNE:
    case Operation::BLT:
    case Operation::BGE:
    case Operation::BLTU:
    case Operation::BGEU: return 0U;
    case Operation::ADDI: return lhs + imm_u;
    case Operation::SLTI: return signed_value(lhs) < imm_s ? 1U : 0U;
    case Operation::SLTIU: return lhs < imm_u ? 1U : 0U;
    case Operation::XORI: return lhs ^ imm_u;
    case Operation::ORI: return lhs | imm_u;
    case Operation::ANDI: return lhs & imm_u;
    case Operation::SLLI: return lhs << (imm_u & 0x1FU);
    case Operation::SRLI: return lhs >> (imm_u & 0x1FU);
    case Operation::SRAI: return arithmetic_shift_right(lhs, imm_u);
    case Operation::ADD: return lhs + rhs;
    case Operation::SUB: return lhs - rhs;
    case Operation::SLL: return lhs << (rhs & 0x1FU);
    case Operation::SLT: return signed_value(lhs) < signed_value(rhs) ? 1U : 0U;
    case Operation::SLTU: return lhs < rhs ? 1U : 0U;
    case Operation::XOR: return lhs ^ rhs;
    case Operation::SRL: return lhs >> (rhs & 0x1FU);
    case Operation::SRA: return arithmetic_shift_right(lhs, rhs);
    case Operation::OR: return lhs | rhs;
    case Operation::AND: return lhs & rhs;
    default: return 0U;
    }
}

bool TomasuloCpu::branch_taken(Operation op, uint32_t lhs, uint32_t rhs) noexcept {
    switch (op) {
    case Operation::BEQ: return lhs == rhs;
    case Operation::BNE: return lhs != rhs;
    case Operation::BLT: return signed_value(lhs) < signed_value(rhs);
    case Operation::BGE: return signed_value(lhs) >= signed_value(rhs);
    case Operation::BLTU: return lhs < rhs;
    case Operation::BGEU: return lhs >= rhs;
    default: return false;
    }
}

int TomasuloCpu::find_free_rs(const State& state) noexcept {
    for (uint8_t i = 0U; i < RsSize; ++i) {
        if (!state.rs[i].busy) return i;
    }
    return -1;
}

int TomasuloCpu::find_oldest_ready_rs(const State& state) noexcept {
    int selected = -1;
    uint64_t oldest = 0U;
    for (uint8_t i = 0U; i < RsSize; ++i) {
        const RsEntry& entry = state.rs[i];
        if (!entry.busy || !entry.lhs.ready || !entry.rhs.ready) continue;
        if (selected < 0 || entry.sequence < oldest) {
            selected = i;
            oldest = entry.sequence;
        }
    }
    return selected;
}

int TomasuloCpu::find_free_lsq(const State& state) noexcept {
    for (uint8_t i = 0U; i < LsqSize; ++i) {
        if (!state.lsq[i].busy) return i;
    }
    return -1;
}

bool TomasuloCpu::has_older_store(const State& state, uint64_t sequence) noexcept {
    for (uint8_t i = 0U; i < RobSize; ++i) {
        const RobEntry& entry = state.rob[i];
        if (entry.busy && entry.sequence < sequence && is_store(entry.instruction.op)) return true;
    }
    return false;
}

bool TomasuloCpu::has_unresolved_control(const State& state) noexcept {
    for (uint8_t i = 0U; i < RobSize; ++i) {
        const RobEntry& entry = state.rob[i];
        if (entry.busy && is_control(entry.instruction.op) && !entry.control_resolved) return true;
    }
    return false;
}

int TomasuloCpu::find_oldest_ready_lsq(const State& state) noexcept {
    int selected = -1;
    uint64_t oldest = 0U;
    for (uint8_t i = 0U; i < LsqSize; ++i) {
        const LsqEntry& entry = state.lsq[i];
        if (!entry.busy || !entry.address.ready) continue;
        if (is_store(entry.instruction.op) && !entry.data.ready) continue;
        if (is_load(entry.instruction.op) && has_older_store(state, entry.sequence)) continue;
        if (selected < 0 || entry.sequence < oldest) {
            selected = i;
            oldest = entry.sequence;
        }
    }
    return selected;
}


void TomasuloCpu::accept_writeback(Operand& operand, const WritebackAction& writeback) noexcept {
    if (!writeback.valid || operand.ready || operand.tag != writeback.rob_tag) return;
    operand.ready = true;
    operand.value = writeback.value;
    operand.tag = NoTag;
}

TomasuloCpu::Operand TomasuloCpu::read_operand(const State& state, uint8_t index, bool used) const noexcept {
    Operand operand{};
    if (!used || index == 0U) return operand;
    if (!state.rat_busy[index]) {
        operand.value = state.registers.read(index);
        return operand;
    }
    const uint8_t tag = state.rat_tag[index];
    const RobEntry& producer = state.rob[tag];
    if (producer.busy && producer.ready) {
        operand.value = producer.value;
    }
    else {
        operand.ready = false;
        operand.tag = tag;
    }
    return operand;
}

uint32_t TomasuloCpu::read_load(Operation op, uint32_t address) const noexcept {
    switch (op) {
    case Operation::LB: return sign_extend(memory_.read_byte(address), 8U);
    case Operation::LH: return sign_extend(memory_.read_half(address), 16U);
    case Operation::LW: return memory_.read_word(address);
    case Operation::LBU: return memory_.read_byte(address);
    case Operation::LHU: return memory_.read_half(address);
    default: return 0U;
    }
}

uint32_t TomasuloCpu::predicted_pc(const State& state, const DecodedInstruction& instruction,
                                   const Operand& lhs) const noexcept {
    const uint32_t next_pc = state.pc + 4U;
    const uint32_t imm = static_cast<uint32_t>(instruction.imm);
    if (is_branch(instruction.op)) {
        const uint16_t index = static_cast<uint16_t>((state.pc >> 2U) & (PredictorSize - 1U));
        return state.predictor[index] >= 2U ? state.pc + imm : next_pc;
    }
    if (instruction.op == Operation::JAL) return state.pc + imm;
    if (instruction.op == Operation::JALR && lhs.ready) return (lhs.value + imm) & ~1U;
    return next_pc;
}

TomasuloCpu::CommitAction TomasuloCpu::commit_module(const State& state) const noexcept {
    CommitAction action{};
    if (state.rob_count == 0U) return action;
    const RobEntry& entry = state.rob[state.rob_head];
    if (!entry.ready) return action;
    action.valid = true;
    action.rob_tag = state.rob_head;
    action.instruction = entry.instruction;
    action.pc = entry.pc;
    action.value = entry.value;
    action.address = entry.address;
    action.store_data = entry.store_data;
    action.predicted_pc = entry.predicted_pc;
    action.actual_pc = entry.actual_pc;
    action.actual_taken = entry.actual_taken;
    return action;
}

TomasuloCpu::WritebackAction TomasuloCpu::writeback_module(const State& state) const noexcept {
    WritebackAction action{};
    const bool alu_ready = state.alu.busy;
    const bool memory_ready = state.memory.busy && state.memory.completed && is_load(state.memory.op);
    if (!alu_ready && !memory_ready) return action;
    if (alu_ready && (!memory_ready || state.alu.sequence <= state.memory.sequence)) {
        action.valid = true;
        action.source = WritebackSource::Alu;
        action.rob_tag = state.alu.rob_tag;
        action.value = state.alu.value;
        action.sequence = state.alu.sequence;
        return action;
    }
    action.valid = true;
    action.source = WritebackSource::Memory;
    action.rob_tag = state.memory.rob_tag;
    action.value = state.memory.value;
    action.sequence = state.memory.sequence;
    return action;
}

TomasuloCpu::ExecuteAction TomasuloCpu::execute_module(const State& state) const noexcept {
    ExecuteAction action{};
    if (state.alu.busy) return action;
    const int index = find_oldest_ready_rs(state);
    if (index < 0) return action;
    const RsEntry& entry = state.rs[index];
    action.valid = true;
    action.rs_index = index;
    action.rob_tag = entry.rob_tag;
    action.value = execute_value(entry);
    action.sequence = entry.sequence;
    if (!is_control(entry.instruction.op)) return action;
    action.control = true;
    if (is_branch(entry.instruction.op)) {
        action.actual_taken = branch_taken(entry.instruction.op, entry.lhs.value, entry.rhs.value);
        action.actual_pc = action.actual_taken
                               ? entry.pc + static_cast<uint32_t>(entry.instruction.imm)
                               : entry.pc + 4U;
    }
    else if (entry.instruction.op == Operation::JAL) {
        action.actual_taken = true;
        action.actual_pc = entry.pc + static_cast<uint32_t>(entry.instruction.imm);
    }
    else {
        action.actual_taken = true;
        action.actual_pc = (entry.lhs.value + static_cast<uint32_t>(entry.instruction.imm)) & ~1U;
    }
    action.mispredict = action.actual_pc != entry.predicted_pc;
    return action;
}

TomasuloCpu::MemoryDispatchAction TomasuloCpu::memory_dispatch_module(const State& state) const noexcept {
    MemoryDispatchAction action{};
    if (state.memory.busy) return action;
    const int index = find_oldest_ready_lsq(state);
    if (index < 0) return action;
    const LsqEntry& entry = state.lsq[index];
    action.valid = true;
    action.lsq_index = index;
    action.rob_tag = entry.rob_tag;
    action.op = entry.instruction.op;
    action.address = entry.address.value + static_cast<uint32_t>(entry.instruction.imm);
    action.store_data = entry.data.value;
    action.sequence = entry.sequence;
    return action;
}

TomasuloCpu::MemoryProgressAction TomasuloCpu::memory_progress_module(const State& state) const noexcept {
    MemoryProgressAction action{};
    if (!state.memory.busy || state.memory.completed) return action;
    action.valid = true;
    action.rob_tag = state.memory.rob_tag;
    action.address = state.memory.address;
    action.store_data = state.memory.store_data;
    action.sequence = state.memory.sequence;
    if (state.memory.remaining > 1U) {
        action.next_remaining = state.memory.remaining - 1U;
        return action;
    }
    action.complete = true;
    if (is_store(state.memory.op)) {
        action.store_complete = true;
    }
    else {
        action.value = read_load(state.memory.op, state.memory.address);
    }
    return action;
}

TomasuloCpu::IssueResult TomasuloCpu::issue_module(const State& state) const {
    IssueResult result{};
    if (state.fetch_stopped) return result;
    const uint32_t raw = memory_.read_word(state.pc);
    if (raw == HaltInstruction) {
        result.action.halt = true;
        return result;
    }
    const DecodedInstruction instruction = Decoder::decode(raw);
    if (!instruction.valid()) {
        if (!has_unresolved_control(state)) result.result = TomasuloStepResult::InvalidInstruction;
        return result;
    }
    if (state.rob_count == RobSize) return result;
    const bool memory = is_memory(instruction.op);
    const int index = memory ? find_free_lsq(state) : find_free_rs(state);
    if (index < 0) return result;
    const Operand lhs = read_operand(state, instruction.rs1, instruction.uses_rs1);
    const Operand rhs = read_operand(state, instruction.rs2, instruction.uses_rs2);
    result.action.valid = true;
    result.action.memory = memory;
    result.action.index = index;
    result.action.rob_tag = state.rob_tail;
    result.action.pc = state.pc;
    result.action.instruction = instruction;
    result.action.lhs = lhs;
    result.action.rhs = rhs;
    result.action.predicted_pc = predicted_pc(state, instruction, lhs);
    result.action.sequence = state.next_sequence;
    return result;
}

void TomasuloCpu::update_rob(State& next, const State& current, const CommitAction& commit,
                             const WritebackAction& writeback,
                             const ExecuteAction& execute, const MemoryProgressAction& progress,
                             const IssueAction& issue,
                             const ExecuteAction& recovery) noexcept {
    const bool reco = recovery.mispredict;
    const uint64_t cutoff = recovery.sequence;
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < RobSize; ++i) {
        RobEntry entry = current.rob[i];
        if (writeback.valid && writeback.rob_tag == i && entry.busy) {
            if (!reco || writeback.sequence <= cutoff) {
                entry.value = writeback.value;
                entry.ready = true;
            }
        }
        if (execute.valid && execute.control && execute.rob_tag == i && entry.busy) {
            entry.control_resolved = true;
            entry.actual_pc = execute.actual_pc;
            entry.actual_taken = execute.actual_taken;
        }
        if (progress.valid && progress.complete && progress.store_complete &&
            progress.rob_tag == i && entry.busy) {
            if (!reco || progress.sequence <= cutoff) {
                entry.address = progress.address;
                entry.store_data = progress.store_data;
                entry.ready = true;
            }
        }
        if (commit.valid && commit.rob_tag == i) entry = {};
        if (reco && entry.busy && entry.sequence > cutoff) entry = {};
        next.rob[i] = entry;
        if (entry.busy) ++count;
    }
    if (issue.valid && !reco) {
        RobEntry entry{};
        entry.busy = true;
        entry.instruction = issue.instruction;
        entry.pc = issue.pc;
        entry.predicted_pc = issue.predicted_pc;
        entry.sequence = issue.sequence;
        next.rob[issue.rob_tag] = entry;
        ++count;
    }
    next.rob_head = commit.valid ? (current.rob_head + 1U) % RobSize : current.rob_head;
    next.rob_tail = reco
                        ? (recovery.rob_tag + 1U) % RobSize
                        : issue.valid
                        ? (current.rob_tail + 1U) % RobSize
                        : current.rob_tail;
    next.rob_count = count;
}

void TomasuloCpu::update_rs(State& next, const State& current, const WritebackAction& writeback,
                            const ExecuteAction& execute, const IssueAction& issue,
                            const ExecuteAction& recovery) noexcept {
    const bool reco = recovery.mispredict;
    const uint64_t cutoff = recovery.sequence;
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < RsSize; ++i) {
        if (!current.rs[i].busy) {
            next.rs[i] = {};
            continue;
        }
        if ((reco && current.rs[i].sequence > cutoff) ||
            (execute.valid && execute.rs_index == i)) {
            next.rs[i] = {};
            continue;
        }
        RsEntry entry = current.rs[i];
        accept_writeback(entry.lhs, writeback);
        accept_writeback(entry.rhs, writeback);
        next.rs[i] = entry;
        ++count;
    }
    if (issue.valid && !issue.memory && !reco) {
        RsEntry entry{};
        entry.busy = true;
        entry.instruction = issue.instruction;
        entry.lhs = issue.lhs;
        entry.rhs = issue.rhs;
        entry.pc = issue.pc;
        entry.predicted_pc = issue.predicted_pc;
        entry.rob_tag = issue.rob_tag;
        entry.sequence = issue.sequence;
        accept_writeback(entry.lhs, writeback);
        accept_writeback(entry.rhs, writeback);
        next.rs[issue.index] = entry;
        ++count;
    }
    next.rs_count = count;
}

void TomasuloCpu::update_lsq(State& next, const State& current, const WritebackAction& writeback,
                             const MemoryDispatchAction& dispatch, const IssueAction& issue,
                             const ExecuteAction& recovery) noexcept {
    const bool reco = recovery.mispredict;
    const uint64_t cutoff = recovery.sequence;
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < LsqSize; ++i) {
        if (!current.lsq[i].busy) {
            next.lsq[i] = {};
            continue;
        }
        if ((reco && current.lsq[i].sequence > cutoff) ||
            (dispatch.valid && dispatch.lsq_index == i &&
             (!reco || dispatch.sequence <= cutoff))) {
            next.lsq[i] = {};
            continue;
        }
        LsqEntry entry = current.lsq[i];
        accept_writeback(entry.address, writeback);
        accept_writeback(entry.data, writeback);
        next.lsq[i] = entry;
        ++count;
    }
    if (issue.valid && issue.memory && !reco) {
        LsqEntry entry{};
        entry.busy = true;
        entry.instruction = issue.instruction;
        entry.address = issue.lhs;
        entry.data = issue.rhs;
        entry.rob_tag = issue.rob_tag;
        entry.sequence = issue.sequence;
        accept_writeback(entry.address, writeback);
        accept_writeback(entry.data, writeback);
        next.lsq[issue.index] = entry;
        ++count;
    }
    next.lsq_count = count;
}

void TomasuloCpu::update_rat(State& next, const State& current, const CommitAction& commit,
                             const IssueAction& issue, const ExecuteAction& recovery) noexcept {
    const bool reco = recovery.mispredict;
    const uint64_t cutoff = recovery.sequence;
    if (reco) {
        for (uint8_t i = 0U; i < RegisterCount; ++i) {
            next.rat_busy[i] = false;
            next.rat_tag[i] = NoTag;
        }
        for (uint8_t offset = 0U; offset < current.rob_count; ++offset) {
            const uint8_t tag = (current.rob_head + offset) % RobSize;
            const RobEntry& entry = current.rob[tag];
            if (!entry.busy || entry.sequence > cutoff) continue;
            if (commit.valid && tag == commit.rob_tag) continue;
            if (writes_register(entry.instruction)) {
                next.rat_busy[entry.instruction.rd] = true;
                next.rat_tag[entry.instruction.rd] = tag;
            }
        }
        return;
    }
    for (uint8_t i = 0U; i < RegisterCount; ++i) {
        next.rat_busy[i] = current.rat_busy[i];
        next.rat_tag[i] = current.rat_tag[i];
    }
    if (commit.valid && writes_register(commit.instruction)) {
        const uint8_t rd = commit.instruction.rd;
        if (current.rat_busy[rd] && current.rat_tag[rd] == commit.rob_tag) {
            next.rat_busy[rd] = false;
            next.rat_tag[rd] = NoTag;
        }
    }
    if (issue.valid && writes_register(issue.instruction)) {
        const uint8_t rd = issue.instruction.rd;
        next.rat_busy[rd] = true;
        next.rat_tag[rd] = issue.rob_tag;
    }
}

void TomasuloCpu::update_units(State& next, const State& current, const WritebackAction& writeback,
                               const ExecuteAction& execute, const MemoryDispatchAction& dispatch,
                               const MemoryProgressAction& progress, const ExecuteAction& recovery) noexcept {
    const bool reco = recovery.mispredict;
    const uint64_t cutoff = recovery.sequence;

    AluUnit alu = current.alu;
    if (current.alu.busy && reco && current.alu.sequence > cutoff) alu = {};
    if (writeback.valid && writeback.source == WritebackSource::Alu &&
        (!reco || writeback.sequence <= cutoff)) {
        alu = {};
    }
    if (execute.valid && (!reco || execute.sequence <= cutoff)) {
        alu.busy = true;
        alu.rob_tag = execute.rob_tag;
        alu.value = execute.value;
        alu.sequence = execute.sequence;
    }
    MemoryUnit memory = current.memory;
    if (current.memory.busy && reco && current.memory.sequence > cutoff) {
        memory = {};
    }
    else if (writeback.valid && writeback.source == WritebackSource::Memory &&
        (!reco || writeback.sequence <= cutoff)) {
        memory = {};
    }
    else if (progress.valid && (!reco || progress.sequence <= cutoff)) {
        if (progress.complete) {
            if (progress.store_complete) {
                memory = {};
            }
            else {
                memory.completed = true;
                memory.value = progress.value;
                memory.remaining = 0U;
            }
        }
        else {
            memory.remaining = progress.next_remaining;
        }
    }
    else if (dispatch.valid && (!reco || dispatch.sequence <= cutoff)) {
        memory = {};
        memory.busy = true;
        memory.op = dispatch.op;
        memory.rob_tag = dispatch.rob_tag;
        memory.address = dispatch.address;
        memory.store_data = dispatch.store_data;
        memory.remaining = MemoryDelay;
        memory.sequence = dispatch.sequence;
    }
    next.alu = alu;
    next.memory = memory;
}

void TomasuloCpu::update_registers(State& next, const State& current, const CommitAction& commit) noexcept {
    RegisterFile registers = current.registers;
    if (commit.valid && writes_register(commit.instruction)) registers.write(commit.instruction.rd, commit.value);
    registers.write(0U, 0U);
    next.registers = registers;
}

void TomasuloCpu::update_control(State& next, const State& current, const CommitAction& commit,
                                 const IssueAction& issue, const ExecuteAction& recovery) noexcept {
    const bool reco = recovery.mispredict;
    next.pc = current.pc;
    next.next_sequence = current.next_sequence;
    next.fetch_stopped = current.fetch_stopped;
    next.cycle_count = current.cycle_count + 1U;
    uint64_t prediction_count = current.prediction_count;
    uint64_t correct_prediction_count = current.correct_prediction_count;

    for (uint16_t i = 0U; i < PredictorSize; ++i) next.predictor[i] = current.predictor[i];
    if (commit.valid && is_branch(commit.instruction.op)) {
        const uint16_t index = static_cast<uint16_t>((commit.pc >> 2U) & (PredictorSize - 1U));
        uint8_t counter = current.predictor[index];
        if (commit.actual_taken) {
            if (counter < 3U) ++counter;
        }
        else if (counter > 0U) {
            --counter;
        }
        next.predictor[index] = counter;
        ++prediction_count;
        if (commit.predicted_pc == commit.actual_pc) ++correct_prediction_count;
    }
    next.prediction_count = prediction_count;
    next.correct_prediction_count = correct_prediction_count;

    if (reco) {
        next.pc = recovery.actual_pc;
        next.fetch_stopped = false;
        return;
    }
    if (issue.valid) {
        next.pc = issue.predicted_pc;
        next.next_sequence = current.next_sequence + 1U;
    }
    if (issue.halt) next.fetch_stopped = true;
}

void TomasuloCpu::commit_store(const CommitAction& commit) noexcept {
    if (!commit.valid || !is_store(commit.instruction.op)) return;
    switch (commit.instruction.op) {
    case Operation::SB:
        memory_.write_byte(commit.address, static_cast<uint8_t>(commit.store_data));
        break;
    case Operation::SH:
        memory_.write_half(commit.address, static_cast<uint16_t>(commit.store_data));
        break;
    case Operation::SW:
        memory_.write_word(commit.address, commit.store_data);
        break;
    default:
        break;
    }
}

bool TomasuloCpu::drained(const State& state) noexcept {
    return state.rob_count == 0U && state.rs_count == 0U && state.lsq_count == 0U &&
        !state.alu.busy && !state.memory.busy;
}

TomasuloStepResult TomasuloCpu::step_cycle() {
    const int default_order[6] = {0, 1, 2, 3, 4, 5};
    return step_cycle_with_order(default_order);
}

TomasuloStepResult TomasuloCpu::step_cycle_with_order(const int order[6]) {
    IssueResult issue{};
    ExecuteAction execute{};
    CommitAction commit{};
    WritebackAction writeback{};
    MemoryDispatchAction dispatch{};
    MemoryProgressAction progress{};

    for (int i = 0; i < 6; ++i) {
        switch (order[i]) {
        case 0: issue = issue_module(state_); break;
        case 1: execute = execute_module(state_); break;
        case 2: commit = commit_module(state_); break;
        case 3: writeback = writeback_module(state_); break;
        case 4: dispatch = memory_dispatch_module(state_); break;
        case 5: progress = memory_progress_module(state_); break;
        }
    }

    State next = state_;
    update_rob(next, state_, commit, writeback, execute, progress, issue.action, execute);
    update_rs(next, state_, writeback, execute, issue.action, execute);
    update_lsq(next, state_, writeback, dispatch, issue.action, execute);
    update_rat(next, state_, commit, issue.action, execute);
    update_units(next, state_, writeback, execute, dispatch, progress, execute);
    update_registers(next, state_, commit);
    update_control(next, state_, commit, issue.action, execute);
    commit_store(commit);
    state_ = next;

    if (issue.result != TomasuloStepResult::Continue) return issue.result;
    if (state_.fetch_stopped && drained(state_)) return TomasuloStepResult::Halted;
    return TomasuloStepResult::Continue;
}
