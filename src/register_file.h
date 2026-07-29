#pragma once

#include <cstdint>

class RegisterFile {
public:
    uint32_t read(std::uint8_t index) const noexcept {
        if (index == 0U) {
            return 0U;
        }

        return registers_[index];
    }

    void write(
        uint8_t index,
        uint32_t value
    ) noexcept {
        if (index == 0U) {
            return;
        }
        registers_[index] = value;
    }

    void reset() noexcept {
        for (auto& value : registers_) {
            value = 0U;
        }
    }

private:
    uint32_t registers_[32]{};
};