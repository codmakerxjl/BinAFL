#pragma once

#include <vector>
#include <cstdint>
#include <string>

// 字典条目结构
struct AFLDictEntry {
    const uint8_t* data;
    size_t len;
};

// 变异类型枚举
enum class AFLMutateType {
    Bitflip,
    Arith,
    Interest,
    Extras,
    Havoc,
    All // 'All' 将会随机选择一种策略
};

/**
 * @brief 对缓冲区执行一次随机的比特翻转操作 (1, 2, 4, 8, 16, 32位)。
 * @param buffer 对要变异的缓冲区向量的引用。
 */
void afl_mutate_bitflip(std::vector<uint8_t>& buffer);

/**
 * @brief 对缓冲区执行一次随机的算术加/减操作 (8, 16, 32位)。
 * @param buffer 对要变异的缓冲区向量的引用。
 */
void afl_mutate_arith(std::vector<uint8_t>& buffer);

/**
 * @brief 从一个预定义的“感兴趣”值列表中，随机挑选一个来覆盖缓冲区中的某个位置。
 * @param buffer 对要变异的缓冲区向量的引用。
 */
void afl_mutate_interest(std::vector<uint8_t>& buffer);

/**
 * @brief 使用用户提供的字典（extras）来对缓冲区进行变异（覆盖或插入）。
 * @param buffer 对要变异的缓冲区向量的引用。
 * @param extras 包含字典条目的向量。
 */
void afl_mutate_extras(std::vector<uint8_t>& buffer, const std::vector<AFLDictEntry>& extras);

/**
 * @brief “大破坏”阶段：随机地、多次地应用各种不同的变异策略。
 * @param buffer 对要变异的缓冲区向量的引用。
 * @param extras 可选的字典，用于在havoc阶段中进行字典变异。
 */
void afl_mutate_havoc(std::vector<uint8_t>& buffer, const std::vector<AFLDictEntry>& extras = {});

/**
 * @brief 主变异函数分发器。
 * @param buffer 对要变异的缓冲区向量的引用。
 * @param type 要执行的变异类型。如果为All，则随机选择一种。
 * @param extras 用户提供的字典，仅在需要时使用。
 */
void afl_mutate_buffer(std::vector<uint8_t>& buffer, AFLMutateType type, const std::vector<AFLDictEntry>& extras = {});
