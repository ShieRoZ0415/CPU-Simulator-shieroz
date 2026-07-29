//
// Created by 14176 on 2026/7/29.
//

#include "loader.h"

bool is_hex(char c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 'a' && c <= 'f') return true;
    if (c >= 'A' && c <= 'F') return true;
    return  false;
}

uint32_t hex_value(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
}

bool hex_to_32(const std::string& text, uint32_t& value) {
    if (text.empty()) return false;
    value = 0U;
    for (char c: text) {
        if (!is_hex(c)) return false;
        value *= 16U;
        value += hex_value(c);
    }
    return true;
}

bool load_data(Memory& memory, std::istream& input) {
    uint32_t address = 0U;
    std::string word;

    while (input >> word) {
        if (word[0] == '@') {
            if (!hex_to_32(word.substr(1), address)) return false;
            continue;
        }

        uint32_t value = 0U;
        if (!hex_to_32(word, value)) return  false;
        if (value > 0xFFU) return false;
        memory.write_byte(address, value);
        ++address;
    }
    return true;
}
