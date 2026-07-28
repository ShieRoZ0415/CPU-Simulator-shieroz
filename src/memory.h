#pragma once

#include <cstdint>
#include <array>

class Memory {
public:
    static constexpr std::size_t MemorySize = 0x500000;

    uint8_t read_byte(uint32_t address) const;
    uint16_t read_half(uint32_t address) const;
    uint32_t read_word(uint32_t address) const;
    void write_byte(uint32_t address, uint8_t value);
    void write_half(uint32_t address, uint16_t value);
    void write_word(uint32_t address, uint32_t value);

private:
    bool check_range(uint32_t address, std::size_t byte_count) const;
    std::array<uint8_t, MemorySize> data_{};
};
