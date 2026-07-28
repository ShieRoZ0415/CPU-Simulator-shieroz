#include "../src/memory.h"

#include <cassert>
#include <cstdint>

int main() {
    Memory memory;

    // byte
    memory.write_byte(0x100, 0xAB);
    assert(memory.read_byte(0x100) == 0xAB);

    // half
    memory.write_half(0x200, 0x1234);
    assert(memory.read_byte(0x200) == 0x34);
    assert(memory.read_byte(0x201) == 0x12);
    assert(memory.read_half(0x200) == 0x1234);

    // word
    memory.write_word(0x300, 0xAABBCCDD);

    assert(memory.read_byte(0x300) == 0xDD);
    assert(memory.read_byte(0x301) == 0xCC);
    assert(memory.read_byte(0x302) == 0xBB);
    assert(memory.read_byte(0x303) == 0xAA);

    assert(memory.read_half(0x300) == 0xCCDD);
    assert(memory.read_half(0x302) == 0xAABB);
    assert(memory.read_word(0x300) == 0xAABBCCDD);

    // 部分覆盖
    memory.write_byte(0x301, 0x11);
    assert(memory.read_word(0x300) == 0xAABB11DD);

    return 0;
}