#pragma once

#include <cstdint>

#include "decoder.h"
#include "memory.h"
#include "register_file.h"

enum class StepResult {
    Continue,
    Halted,
    InvalidInstruction
};

class Cpu {
public:
    explicit Cpu(Memory& memory) noexcept;
    StepResult step();
    void reset() noexcept;
    uint32_t pc() const noexcept;
    uint32_t register_value(uint8_t index) const noexcept;

private:
    static constexpr uint32_t HaltInstruction = 0x0FF00513U;
    static int64_t signed_value(uint32_t value) noexcept;
    static uint32_t sign_extend_to_u32(uint32_t value, unsigned int bit_count) noexcept;
    static uint32_t arithmetic_shift_right(uint32_t value, uint32_t shift_amount) noexcept;
    bool is_memory(Operation op) noexcept;
    Memory& memory_;
    RegisterFile registers_{};
    uint32_t pc_{0};
    uint64_t cycle_count_{0};
    uint64_t cycle_count() const noexcept;
};