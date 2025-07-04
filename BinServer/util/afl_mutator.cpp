#include "afl_mutator.h"
#include <random>
#include <algorithm>
#include <cstring>
#include <vector>

// --- AFL参数宏 ---
constexpr int ARITH_MAX = 35;
constexpr int HAVOC_STACK_POW2 = 7; // 2^7 = 128
constexpr int HAVOC_MIN = 16;
constexpr size_t MAX_FILE = 1024 * 1024;

// --- AFL "interesting" values ---
static const int8_t  interesting_8[] = { -128, -1, 0, 1, 16, 32, 64, 100, 127 };
static const int16_t interesting_16[] = { -32768, -128, -1, 0, 1, 16, 32, 100, 128, 255, 256, 512, 1000, 1024, 4096, 32767 };
static const int32_t interesting_32[] = { -2147483648, -100663046, -32768, -128, -1, 0, 1, 16, 32, 100, 128, 255, 256, 512, 1000, 1024, 4096, 32767, 65535, 100663046, 2147483647 };

// --- 随机数生成器 ---
static std::mt19937 rng{ std::random_device{}() };
static uint32_t UR(uint32_t limit) {
    if (limit == 0) return 0;
    std::uniform_int_distribution<uint32_t> dist(0, limit - 1);
    return dist(rng);
}

// --- 字节序宏 ---
#ifdef _MSC_VER
#include <stdlib.h>
#define SWAP16(x) _byteswap_ushort(x)
#define SWAP32(x) _byteswap_ulong(x)
#else
#define SWAP16(x) __builtin_bswap16(x)
#define SWAP32(x) __builtin_bswap32(x)
#endif


// --- 变异函数实现 ---

void afl_mutate_bitflip(std::vector<uint8_t>& buffer) {
    if (buffer.empty()) return;

    size_t len = buffer.size();
    size_t bit_len = len << 3;
    size_t bit_pos = UR(bit_len);

    switch (UR(3)) {
    case 0: // 1-bit flip
        buffer[bit_pos / 8] ^= (128 >> (bit_pos % 8));
        break;
    case 1: // 2-bit flip
        if (bit_len < 2) break;
        bit_pos = UR(bit_len - 1);
        buffer[bit_pos / 8] ^= (128 >> (bit_pos % 8));
        buffer[(bit_pos + 1) / 8] ^= (128 >> ((bit_pos + 1) % 8));
        break;
    case 2: // 4-bit flip
        if (bit_len < 4) break;
        bit_pos = UR(bit_len - 3);
        for (int i = 0; i < 4; ++i) {
            buffer[(bit_pos + i) / 8] ^= (128 >> ((bit_pos + i) % 8));
        }
        break;
    }
}

void afl_mutate_arith(std::vector<uint8_t>& buffer) {
    if (buffer.empty()) return;
    size_t len = buffer.size();

    size_t pos = UR(len);
    uint8_t val = 1 + UR(ARITH_MAX);
    bool sub = UR(2);

    switch (UR(3)) {
    case 0: // 8-bit
        if (sub) buffer[pos] -= val;
        else buffer[pos] += val;
        break;
    case 1: // 16-bit
        if (len < 2) break;
        pos = UR(len - 1);
        {
            uint16_t* ptr = reinterpret_cast<uint16_t*>(&buffer[pos]);
            uint16_t v = *ptr;
            if (UR(2)) v = SWAP16(v);
            if (sub) v -= val;
            else v += val;
            if (UR(2)) v = SWAP16(v);
            *ptr = v;
        }
        break;
    case 2: // 32-bit
        if (len < 4) break;
        pos = UR(len - 3);
        {
            uint32_t* ptr = reinterpret_cast<uint32_t*>(&buffer[pos]);
            uint32_t v = *ptr;
            if (UR(2)) v = SWAP32(v);
            if (sub) v -= val;
            else v += val;
            if (UR(2)) v = SWAP32(v);
            *ptr = v;
        }
        break;
    }
}

void afl_mutate_interest(std::vector<uint8_t>& buffer) {
    if (buffer.empty()) return;
    size_t len = buffer.size();

    size_t pos = UR(len);

    switch (UR(3)) {
    case 0: // 8-bit
        buffer[pos] = interesting_8[UR(sizeof(interesting_8))];
        break;
    case 1: // 16-bit
        if (len < 2) break;
        pos = UR(len - 1);
        {
            uint16_t val = interesting_16[UR(sizeof(interesting_16) / sizeof(uint16_t))];
            if (UR(2)) val = SWAP16(val);
            *reinterpret_cast<uint16_t*>(&buffer[pos]) = val;
        }
        break;
    case 2: // 32-bit
        if (len < 4) break;
        pos = UR(len - 3);
        {
            uint32_t val = interesting_32[UR(sizeof(interesting_32) / sizeof(uint32_t))];
            if (UR(2)) val = SWAP32(val);
            *reinterpret_cast<uint32_t*>(&buffer[pos]) = val;
        }
        break;
    }
}

void afl_mutate_extras(std::vector<uint8_t>& buffer, const std::vector<AFLDictEntry>& extras) {
    if (buffer.empty() || extras.empty()) return;
    size_t len = buffer.size();

    const auto& entry = extras[UR(extras.size())];
    if (entry.len == 0) return;

    // 随机选择覆盖或插入
    if (UR(2) == 0) { // Overwrite
        if (len < entry.len) return;
        size_t pos = UR(len - entry.len + 1);
        memcpy(&buffer[pos], entry.data, entry.len);
    }
    else { // Insert
        if (len + entry.len > MAX_FILE) return;
        size_t pos = UR(len + 1);
        buffer.insert(buffer.begin() + pos, entry.data, entry.data + entry.len);
    }
}

void afl_mutate_havoc(std::vector<uint8_t>& buffer, const std::vector<AFLDictEntry>& extras) {
    if (buffer.empty()) return;

    size_t use_stacking = 1 << (1 + UR(HAVOC_STACK_POW2));

    for (size_t i = 0; i < use_stacking; i++) {
        switch (UR(5)) {
        case 0: afl_mutate_bitflip(buffer); break;
        case 1: afl_mutate_arith(buffer); break;
        case 2: afl_mutate_interest(buffer); break;
        case 3: if (!extras.empty()) afl_mutate_extras(buffer, extras); break;
        case 4: // Block deletion or duplication
        {
            size_t len = buffer.size();
            if (len < 2) break;
            size_t block_len = 1 + UR(std::min((size_t)HAVOC_MIN, len - 1));
            size_t block_pos = UR(len - block_len + 1);

            if (UR(2) == 0) { // Deletion
                buffer.erase(buffer.begin() + block_pos, buffer.begin() + block_pos + block_len);
            }
            else { // Duplication
                if (len + block_len > MAX_FILE) break;
                buffer.insert(buffer.begin() + UR(len + 1), buffer.begin() + block_pos, buffer.begin() + block_pos + block_len);
            }
            break;
        }
        }
    }
}

void afl_mutate_buffer(std::vector<uint8_t>& buffer, AFLMutateType type, const std::vector<AFLDictEntry>& extras) {
    if (type == AFLMutateType::All) {
        type = static_cast<AFLMutateType>(UR(5)); // 随机选择一种
    }

    switch (type) {
    case AFLMutateType::Bitflip:  afl_mutate_bitflip(buffer); break;
    case AFLMutateType::Arith:    afl_mutate_arith(buffer); break;
    case AFLMutateType::Interest: afl_mutate_interest(buffer); break;
    case AFLMutateType::Extras:   afl_mutate_extras(buffer, extras); break;
    case AFLMutateType::Havoc:    afl_mutate_havoc(buffer, extras); break;
    default: break;
    }
}
