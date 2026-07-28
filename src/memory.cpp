//
// Created by 14176 on 2026/7/27.
//
#include <stdexcept>
#include "memory.h"

bool Memory::check_range(uint32_t address, std::size_t byte_count) const {
    const auto start = static_cast<std::size_t>(address);
    if (byte_count > data_.size()) return false;
    if (start > data_.size() - byte_count) return false;
    return true;
}

uint8_t Memory::read_byte(uint32_t address) const {
    if (!check_range(address, 1)) {
        throw std::out_of_range("Memory::read_byte address out of range");
    }
    return data_[static_cast<std::size_t>(address)];
}

uint16_t Memory::read_half(uint32_t address) const {
    if (!check_range(address, 2)) {
        throw std::out_of_range("Memory::read_half address out of range");
    }
    const auto byte0 = static_cast<uint16_t>(read_byte(address));
    const auto byte1 = static_cast<uint16_t>(read_byte(address + 1U));
    return static_cast<uint16_t>(byte0 | (byte1 << 8U));
}

uint32_t Memory::read_word(uint32_t address) const {
    if (!check_range(address, 4)) {
        throw std::out_of_range("Memory::read_word address out of range");
    }
    const auto byte0 = static_cast<uint32_t>(read_byte(address));
    const auto byte1 = static_cast<uint32_t>(read_byte(address + 1U));
    const auto byte2 = static_cast<uint32_t>(read_byte(address + 2U));
    const auto byte3 = static_cast<uint32_t>(read_byte(address + 3U));
    return static_cast<uint32_t>(byte0 | byte1 << 8U | byte2 << 16U | byte3 << 24U);
}

void Memory::write_byte(uint32_t address, uint8_t value) {
    if (!check_range(address, 1)) {
        throw std::out_of_range("Memory::write_byte address out of range");
    }
    data_[static_cast<std::size_t>(address)] = value;
}

void Memory::write_half(uint32_t address, uint16_t value) {
    if (!check_range(address, 2)) {
        throw std::out_of_range("Memory::write_half address out of range");
    }
    const auto byte0 = static_cast<uint8_t>(value & 0x00FFU);
    const auto byte1 = static_cast<uint8_t>((value >> 8U) & 0x00FFU);
    write_byte(address, byte0);
    write_byte(address + 1U, byte1);
}

void Memory::write_word(uint32_t address, uint32_t value) {
    if (!check_range(address, 4)) {
        throw std::out_of_range("Memory::write_word address out of range");
    }
    const auto byte0 = static_cast<uint8_t>(value & 0x00FFU);
    const auto byte1 = static_cast<uint8_t>((value >> 8U) & 0x00FFU);
    const auto byte2 = static_cast<uint8_t>((value >> 16U) & 0x00FFU);
    const auto byte3 = static_cast<uint8_t>((value >> 24U) & 0x00FFU);
    write_byte(address, byte0);
    write_byte(address + 1U, byte1);
    write_byte(address + 2U, byte2);
    write_byte(address + 3U, byte3);
}


