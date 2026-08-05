#pragma once

#include <cstdint>

#include "decoder.h"
#include "memory.h"
#include "register_file.h"

enum class TomasuloStepResult : uint8_t {
    Continue,
    Halted,
    InvalidInstruction
};

// 乱序执行 CPU, 采用 Tomasulo 算法: ROB + RS + LSQ + 分支预测
// 取指按序, 提交按序, 乱序执行
class TomasuloCpu {
public:
    explicit TomasuloCpu(Memory& memory) noexcept;

    void reset() noexcept;
    TomasuloStepResult step_cycle();
    TomasuloStepResult step_cycle_with_order(const int order[6]);

    uint32_t pc() const noexcept;
    uint32_t register_value(uint8_t index) const noexcept;
    uint64_t cycle_count() const noexcept;
    uint64_t branch_predictions() const noexcept;
    uint64_t correct_branch_predictions() const noexcept;
    double branch_accuracy() const noexcept;

private:
    static constexpr uint32_t HaltInstruction = 0x0FF00513U;
    static constexpr uint8_t RegisterCount = 32U;
    static constexpr uint8_t RobSize = 16U;          // ROB 条目数
    static constexpr uint8_t RsSize = 12U;           // 保留站条目数
    static constexpr uint8_t LsqSize = 12U;          // LSQ 条目数
    static constexpr uint16_t PredictorSize = 256U;   // 分支预测器大小(2-bit)
    static constexpr uint8_t MemoryDelay = 3U;       // 访存延迟周期
    static constexpr uint8_t NoTag = 0xFFU;           // 空标记, 操作数未就绪时的占位

    enum class WritebackSource : uint8_t {
        None,
        Alu,
        Memory
    };

    // 操作数: 带就绪标记, 用于保留站等待源操作数
    struct Operand {
        bool ready{true};
        uint32_t value{0};
        uint8_t tag{NoTag};
    };

    // Reorder Buffer 条目: 记录每条指令的生命周期
    struct RobEntry {
        bool busy{false};
        bool ready{false};
        bool control_resolved{false};
        DecodedInstruction instruction{};
        uint32_t pc{0};
        uint32_t value{0};
        uint32_t address{0};
        uint32_t store_data{0};
        uint32_t predicted_pc{0};
        uint32_t actual_pc{0};
        bool actual_taken{false};
        uint64_t sequence{0};
    };

    // 保留站条目: 等待操作数就绪后发射到 ALU
    struct RsEntry {
        bool busy{false};
        DecodedInstruction instruction{};
        Operand lhs{};
        Operand rhs{};
        uint32_t pc{0};
        uint32_t predicted_pc{0};
        uint8_t rob_tag{NoTag};
        uint64_t sequence{0};
    };

    // LSQ 条目: 记录 load/store 的地址和写数据
    struct LsqEntry {
        bool busy{false};
        DecodedInstruction instruction{};
        Operand address{};
        Operand data{};
        uint8_t rob_tag{NoTag};
        uint64_t sequence{0};
    };

    struct AluUnit {
        bool busy{false};
        uint8_t rob_tag{NoTag};
        uint32_t value{0};
        uint64_t sequence{0};
    };

    struct MemoryUnit {
        bool busy{false};
        bool completed{false};
        Operation op{Operation::Invalid};
        uint8_t rob_tag{NoTag};
        uint32_t address{0};
        uint32_t store_data{0};
        uint32_t value{0};
        uint8_t remaining{0};
        uint64_t sequence{0};
    };

    // 所有状态的集合 (模拟硬件寄存器)
    struct State {
        RegisterFile registers{};

        RobEntry rob[RobSize]{};
        uint8_t rob_head{0};
        uint8_t rob_tail{0};
        uint8_t rob_count{0};

        RsEntry rs[RsSize]{};
        uint8_t rs_count{0};

        LsqEntry lsq[LsqSize]{};
        uint8_t lsq_count{0};

        bool rat_busy[RegisterCount]{};
        uint8_t rat_tag[RegisterCount]{};

        uint8_t predictor[PredictorSize]{};
        uint64_t prediction_count{0};
        uint64_t correct_prediction_count{0};

        AluUnit alu{};
        MemoryUnit memory{};

        uint32_t pc{0};
        uint64_t cycle_count{0};
        uint64_t next_sequence{0};
        bool fetch_stopped{false};
    };

    struct CommitAction {
        bool valid{false};
        uint8_t rob_tag{NoTag};
        DecodedInstruction instruction{};
        uint32_t pc{0};
        uint32_t value{0};
        uint32_t address{0};
        uint32_t store_data{0};
        uint32_t predicted_pc{0};
        uint32_t actual_pc{0};
        bool actual_taken{false};
    };

    struct WritebackAction {
        bool valid{false};
        WritebackSource source{WritebackSource::None};
        uint8_t rob_tag{NoTag};
        uint32_t value{0};
        uint64_t sequence{0};
    };

    struct ExecuteAction {
        bool valid{false};
        bool control{false};
        bool mispredict{false};
        bool actual_taken{false};
        uint8_t rs_index{NoTag};
        uint8_t rob_tag{NoTag};
        uint32_t value{0};
        uint32_t actual_pc{0};
        uint64_t sequence{0};
    };

    struct MemoryDispatchAction {
        bool valid{false};
        uint8_t lsq_index{NoTag};
        uint8_t rob_tag{NoTag};
        Operation op{Operation::Invalid};
        uint32_t address{0};
        uint32_t store_data{0};
        uint64_t sequence{0};
    };

    struct MemoryProgressAction {
        bool valid{false};
        bool complete{false};
        bool store_complete{false};
        uint8_t next_remaining{0};
        uint8_t rob_tag{NoTag};
        uint32_t address{0};
        uint32_t store_data{0};
        uint32_t value{0};
        uint64_t sequence{0};
    };

    struct IssueAction {
        bool valid{false};
        bool halt{false};
        bool memory{false};
        uint8_t index{NoTag};
        uint8_t rob_tag{NoTag};
        uint32_t pc{0};
        uint32_t predicted_pc{0};
        DecodedInstruction instruction{};
        Operand lhs{};
        Operand rhs{};
        uint64_t sequence{0};
    };

    struct IssueResult {
        TomasuloStepResult result{TomasuloStepResult::Continue};
        IssueAction action{};
    };

    static bool writes_register(const DecodedInstruction& instruction) noexcept;
    static bool is_load(Operation op) noexcept;
    static bool is_store(Operation op) noexcept;
    static bool is_memory(Operation op) noexcept;
    static bool is_branch(Operation op) noexcept;
    static bool is_control(Operation op) noexcept;

    static int64_t signed_value(uint32_t value) noexcept;
    static uint32_t sign_extend(uint32_t value, uint32_t bits) noexcept;
    static uint32_t arithmetic_shift_right(uint32_t value, uint32_t amount) noexcept;
    static uint32_t execute_value(const RsEntry& entry) noexcept;
    static bool branch_taken(Operation op, uint32_t lhs, uint32_t rhs) noexcept;

    static int find_free_rs(const State& state) noexcept;
    static int find_oldest_ready_rs(const State& state) noexcept;
    static int find_free_lsq(const State& state) noexcept;
    static int find_oldest_ready_lsq(const State& state) noexcept;
    static bool has_older_store(const State& state, uint64_t sequence) noexcept;
    static bool has_unresolved_control(const State& state) noexcept;
    static void accept_writeback(Operand& operand, const WritebackAction& writeback) noexcept;

    Operand read_operand(const State& state, uint8_t index, bool used) const noexcept;
    uint32_t read_load(Operation op, uint32_t address) const noexcept;
    uint32_t predicted_pc(const State& state, const DecodedInstruction& instruction,
                          const Operand& lhs) const noexcept;

    // 各模块: 从状态计算出本周期要执行的动作, 不修改状态
    CommitAction commit_module(const State& state) const noexcept;
    WritebackAction writeback_module(const State& state) const noexcept;
    ExecuteAction execute_module(const State& state) const noexcept;
    MemoryDispatchAction memory_dispatch_module(const State& state) const noexcept;
    MemoryProgressAction memory_progress_module(const State& state) const noexcept;
    IssueResult issue_module(const State& state) const;

    // 状态更新: 把模块产生的动作应用到下一周期状态

    static void update_rob(State& next, const State& current, const CommitAction& commit,
                           const WritebackAction& writeback, const ExecuteAction& execute,
                           const MemoryProgressAction& progress, const IssueAction& issue,
                           const ExecuteAction& recovery) noexcept;

    static void update_rs(State& next, const State& current, const WritebackAction& writeback,
                          const ExecuteAction& execute, const IssueAction& issue,
                          const ExecuteAction& recovery) noexcept;

    static void update_lsq(State& next, const State& current, const WritebackAction& writeback,
                           const MemoryDispatchAction& dispatch, const IssueAction& issue,
                           const ExecuteAction& recovery) noexcept;

    static void update_rat(State& next, const State& current, const CommitAction& commit,
                           const IssueAction& issue, const ExecuteAction& recovery) noexcept;

    static void update_units(State& next, const State& current, const WritebackAction& writeback,
                             const ExecuteAction& execute, const MemoryDispatchAction& dispatch,
                             const MemoryProgressAction& progress,
                             const ExecuteAction& recovery) noexcept;

    static void update_registers(State& next, const State& current,
                                 const CommitAction& commit) noexcept;

    static void update_control(State& next, const State& current, const CommitAction& commit,
                               const IssueAction& issue,
                               const ExecuteAction& recovery) noexcept;

    void commit_store(const CommitAction& commit) noexcept;
    static bool drained(const State& state) noexcept;

    Memory& memory_;
    State state_{};
};