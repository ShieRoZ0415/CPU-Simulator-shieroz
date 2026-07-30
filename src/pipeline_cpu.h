#pragma once

#pragma once

#include <cstdint>

#include "decoder.h"
#include "memory.h"
#include "register_file.h"

enum class PipelineStepResult {
    Continue,
    Halted,
    InvalidInstruction
};

class PipelineCpu {
public:
    explicit PipelineCpu(Memory& memory) noexcept;
    PipelineStepResult step_cycle();
    void reset() noexcept;
    uint32_t pc() const noexcept;
    uint32_t register_value(uint8_t index) const noexcept;
    uint64_t cycle_count() const noexcept;

private:
    static constexpr uint32_t HaltInstruction = 0x0FF00513U;
    static constexpr uint8_t MemoryCycles = 3U;

    static int64_t signed_value(uint32_t value) noexcept;
    static uint32_t sign_extend_to_u32(uint32_t value, unsigned int bit_count) noexcept;
    static uint32_t arithmetic_shift_right(uint32_t value, uint32_t shift_amount) noexcept;
    Memory& memory_;
    RegisterFile registers_{};
    uint32_t pc_{0};
    uint64_t cycle_count_{0}; // 已运行周期数
    bool fetch_stopped_{false};

    struct IfIdRegister {
        bool valid{false}; // false 表示 bubble
        uint32_t pc{0};
        uint32_t raw{0};
    };

    struct IdExRegister {
        bool valid{false};
        uint32_t pc{0};
        DecodedInstruction instruction{};
        uint32_t rs1_value{0};
        uint32_t rs2_value{0};
    };

    struct ExMemRegister {
        bool valid{false};
        DecodedInstruction instruction{};
        uint32_t result{0};
        uint32_t store_data{0};
        uint8_t mem_cycles{0}; // 访存还需要占用多少个 MEM cycle
    };

    struct MemWbRegister {
        bool valid{false};
        DecodedInstruction instruction{};
        uint32_t write_value{0};
    };

    IfIdRegister if_id_{};
    IdExRegister id_ex_{};
    ExMemRegister ex_mem_{};
    MemWbRegister mem_wb_{};

    static bool is_load(Operation op) noexcept;
    static bool is_memory(Operation op) noexcept;
    static bool write_register(const DecodedInstruction& instruction) noexcept;
    uint32_t forwarded_rs1(const IdExRegister& current) const noexcept;
    uint32_t forwarded_rs2(const IdExRegister& current) const noexcept;
    bool needs_stall(const DecodedInstruction& instruction) const noexcept;
};